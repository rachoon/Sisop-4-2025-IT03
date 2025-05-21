# Modul 4

## soal_1

## soal_2
Pada soal ini, program dapat menyatukan beberapa file yang terpecah menjadi 14 bagian dengan format `.000`, `.001`, sampai `.013`. 

### A. `baymax_getattr()`
Pertama-tama, kita perlu mendapatkan atribut file berdasarkan path yuang diminta. Untuk kodenya seperti ini

```
static int baymax_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
    } else {
        char *filename = basename(strdup(path + 1));
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s.000", options.relics_dir, filename);

        if (access(fragment_path, F_OK) == 0) {
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;

            size_t total_size = 0;
            int fragment_count = 0;
            char current_fragment_path[PATH_MAX];

            while (1) {
                snprintf(current_fragment_path, sizeof(current_fragment_path), "%s/%s.%03d", options.relics_dir, filename, fragment_count);
                if (access(current_fragment_path, F_OK) == 0) {
                    struct stat fragment_stat;
                    if (stat(current_fragment_path, &fragment_stat) == 0) {
                        total_size += fragment_stat.st_size;
                    }
                    fragment_count++;
                } else {
                    break;
                }
            }
            stbuf->st_size = total_size;

        } else {
            res = -ENOENT;
        }
        free(filename);
    }
    return res;
}
```
Untuk cara kerjanya seperti ini.
1. Untuk mencegah terjadinya error, diperlukan `memset` untuk membersihkan memori.
2. Jika program menangani direktori root, maka permission diubah menjadi 0777.
3. Jika program menangani file, maka program akan mencari atribut file berupa informasi yang terkait dengan file tersebut.
4. Jika file ada, maka program dapat menjalankan fungsi `if (access(fragment_path, F_OK) == 0)`.
5. Setelah itu, file virtual dibuat di dalam fungsi `while(1)`.
6. Jika file tidak ada, maka akan muncul error dengan kode `-ENOENT`.
7. Memori dibersihakn untuk menghindari terjadinya memory leak.
8. Program akan return 0 jika berhasil (`return res`).

### B. `baymax_readdir()`
Selanjutnya, program akan membaca file dengan fungsi ini. Untuk kodenya seperti ini
```
static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;
    
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
    DIR *dir;
    struct dirent *ent;
    char *files[MAX_FRAGMENTS];
    int file_count = 0;
    
    if ((dir = opendir(options.relics_dir))) {
        while ((ent = readdir(dir)) && file_count < MAX_FRAGMENTS) {
            char *dot = strrchr(ent->d_name, '.');
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) {
                *dot = '\0';
                int found = 0;
                for (int i = 0; i < file_count; i++) {
                    if (strcmp(files[i], ent->d_name) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    files[file_count] = strdup(ent->d_name);
                    file_count++;
                }
                *dot = '.';
            }
        }
        closedir(dir);
        
        for (int i = 0; i < file_count; i++) {
            filler(buf, files[i], NULL, 0);
            free(files[i]);
        }
    }
    return 0;
}
```

Dimana untuk cara kerjanya seperti ini.
1. Untuk mencegah terjadinya error, diperlukan `(void)` untuk paramter yang mungkin tidak dimasukkan.
2. Pengecekan path dilakukan, jika tidak ada maka fungsi akan mengembalikan error berupa `-ENOENT`.
3. Fungsi `filler(buf, ".", NULL, 0);` diperlukan untuk menambahkan nama entri ke dalam buffer yang dikembalikan oleh FUSE.
4. Setelah itu, fungsi membaca direktori `relics`.
5. Lalu, fungsi `while()` dijalankan untuk membaca setiap entri hingga tidak ada lagi entri yang harus dibaca.
6. `char *dot = strrchr(ent->d_name, '.');` digunakan untuk mencari kemunculan katakter terskhir, misalnya `.000`, `.001`, dan seterusnya.
7. Lalu, fungsi `if()` dijalankan. Jika syarat terpenuhi, maka karakter terakhir ditambahkan null terminator berupa `'\0'`.
8. Karena sebuah file virtual mungkin terdiri dari banyak fragmen (misalnya `document.000`, `document.001`, `document.002`), kita tidak ingin menampilkan document berkali-kali di direktori FUSE. Kode ini memeriksa apakah nama file (setelah dipotong ekstensi fragmen) sudah ada di array files yang menyimpan nama-nama unik. Jika belum ada, nama tersebut disalin ke files dan file_count ditingkatkan.
9. Setelah loop pembacaan direktori `relics` selesai dan semua file unik dikumpulakn di array `files`, maka kode `filler(buf, files[i], NULL, 0)` digunakan untuk menambahkan nama file ke buffer yang akan dikembalikan oleh FUSE.
10. `free(files[i]` digunakan untuk membersihkan memori untuk menghindari terjadinya memory leak.

### C. `baymax_open()`
Pada fungsi ini, program akan membuka file virtual dalam FUSE. Untuk kodenya seperti ini.
```
static int baymax_open(const char *path, struct fuse_file_info *fi) {
    char *filename = basename(strdup(path + 1));
    log_activity("READ", filename);
    free(filename);
    return 0;
}
```
Dimana untuk log_activity akan dijabarkan pada poin K.

### D. `baymax_read()`
Fungsi ini diperlukan untuk membaca file. Perbedaannya dengan `baymax_readdir` terletak pada jenis entitas yang dibaca. pada `baymax_readdir`, FUSE akan membaca sebuah dir, sedangkan pada `baymax_read` akan membaca sebuah file. Untuk kodenya seperti ini
```
static int baymax_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    char *fragments[MAX_FRAGMENTS];
    char *filename = basename(strdup(path + 1));
    int count = get_fragments(filename, fragments);
    
    if (count == 0) {
        free(filename);
        return -ENOENT;
    }

    size_t total_size = 0;
    size_t fragment_sizes[MAX_FRAGMENTS];

    for (int i = 0; i < count; i++) {
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s", options.relics_dir, fragments[i]);
        struct stat st;
        if (stat(fragment_path, &st) == 0) {
            fragment_sizes[i] = st.st_size;
            total_size += st.st_size;
        } else {
            fragment_sizes[i] = 0;
        }
    }

    size_t total_read = 0;
    size_t current_offset = 0;

    for (int i = 0; i < count && total_read < size; i++) {
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s", options.relics_dir, fragments[i]);
        
        FILE *f = fopen(fragment_path, "rb");
        if (f) {
            size_t fragment_size = fragment_sizes[i];
            if (offset < current_offset + fragment_size) {
                size_t fragment_offset = (offset > current_offset) ? offset - current_offset : 0;
                size_t read_size = fragment_size - fragment_offset;
                if (read_size > size - total_read)
                    read_size = size - total_read;

                if (fragment_offset < fragment_size) {
                    fseek(f, fragment_offset, SEEK_SET);
                    total_read += fread(buf + total_read, 1, read_size, f);
                }
            }
            current_offset += fragment_size;
            fclose(f);
        }
        free(fragments[i]);
    }

    free(filename);
    return total_read;
}
```
Dimana cara kerjanya sebagai berikut.
1. `memset(stbuf, 0, sizeof(struct stat));` digunakan untuk membersihkan memori agar tidak ada sampah yang mengganggu jalannya kode.
2. Jika path yang diminta adalah "/", artinya sistem operasi menanyakan atribut untuk direktori root.
3. Jika path adalah file, ia mendapatkan nama file dasar dan memeriksa apakah fragmen pertamanya (.000) ada di direktori relics.
4. Apabila fragmen .000 ditemukan, ia mulai menghitung ukuran total file virtual dengan menjumlahkan ukuran semua fragmen (.000, .001, dst.) yang terkait dengan nama file dasar tersebut.
5. Fungsi akan menghitung ukuran file virtual dengan menjumlahkan semua fragmen yang terkait dengan nama file tersebut.
6. Jika proses read selesai, maka memori dibersihkan untuk menghindari terjadinya memory leak.
7. Fungsi akan me-return 0 jika berhasil.

### E. `baymax_unink()`
Fungsi ini digunakan untuk menghapus file yang ada di mountpoint. Jika file ini dihapus, maka file fisik yang terkait juga dihapus. Untuk kodenya seperti ini
```
static int baymax_unlink(const char *path) {
    char *fragments[MAX_FRAGMENTS];
    char *filename = basename(strdup(path + 1));
    int count = get_fragments(filename, fragments);
    if (count == 0) {
        free(filename);
        return -ENOENT;
    }
    char details[1024] = {0};
    strcat(details, fragments[0]);
    for (int i = 1; i < count; i++) {
        strcat(details, " - ");
        strcat(details, fragments[i]);
    }
    log_activity("DELETE", details);
    for (int i = 0; i < count; i++) {
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s", options.relics_dir, fragments[i]);
        unlink(fragment_path);
        free(fragments[i]);
    }
    free(filename);
    return 0;
}
```
Dimana cara kerjanya sebagai berikut.
1. Program mendapatkan nama file dasar dari jalur yang diminta, lalu mencari semua fragmen fisik yang menyusun file virtual tersebut.
2. Jika fragmen tidak ditemukan, maka berarti file virtual tidak ada dan program mengembalikan error berupa `-ENOENT`.
3. Fungsi mencatat aktivitas "DELETE" ke file log.
4. Program kemudian mengiterasi setiap fragmen, membangun jalur lengkapnya, dan memanggil fungsi `unlink()` untuk menghapus fragmen fisik tersebut dari disk.
5. Setelah semua fragmen dihapus, memori yang dialokasikan akan dibebaskan untuk menghindari terjadinya memory leak.
6. Fungsi mengembalikan 0 jika penghapusan file di mountpoint berhasil.

### F. `baymax_create()`
Fungsi ini digunakan untuk membuat file, untuk kodenya seperti ini
```
static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char *filename = basename(strdup(path + 1));
    char fragment_path[PATH_MAX];
    snprintf(fragment_path, sizeof(fragment_path), "%s/%s.000", options.relics_dir, filename);
    int fd = open(fragment_path, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (fd == -1) {
        free(filename);
        return -errno;
    }
    close(fd);
    log_activity("CREATE", filename);
    free(filename);
    return 0;
}
```
Dimana untuk cara kerjanya sebagai berikut.
1. Fungsi mendapatkan nama file dasar yang diminta untuk dibuat di path.
2. Fungsi membuat path lengkap.
3. Fungsi `open()` dipanggil untuk membuat file fisik fragmen `.000` di lokasi `relics`, dengan bendera O_CREAT (buat jika tidak ada), `O_EXCL` (error jika sudah ada), dan `O_WRONLY` (hanya tulis), serta mode izin yang diberikan (`mode`).
4. Jika `fd` bernilai `-1`, maka memori dibersihkan dan mengembalikan error berupa `-errno`.
5. Setleh file berhasil dibuat, maka `fd` akan ditutup menggunakan `close(fd)`.
6. Setelah itu, fungsi mencatat log jika berhasil.
7. Terakhir, memori dibersihkan untuk menghindari terjadinya memory leak.

### G. `baymax_truncate()`
Fungsi ini digunakan untuk mengubah ukuran file. Untuk kodenya sepertii ini.
```
static int baymax_truncate(const char *path, off_t size) {
 char *filename = basename(strdup(path + 1));
    char base_filename[256];
    strncpy(base_filename, filename, sizeof(base_filename) - 1);
    base_filename[sizeof(base_filename) - 1] = '\0';
    char fragment_path[PATH_MAX];
    snprintf(fragment_path, sizeof(fragment_path), "%s/%s.000", options.relics_dir, base_filename);

    int res = truncate(fragment_path, size);
     if (res == -1) {
        free(filename);
        return -errno;
    }
    log_activity("TRUNCATE", filename);
    free(filename);
    return 0;
}
```
Dimana untuk cara kerjanya sebagai berikut.
1. Program mendapatkan nama file dasar yang diminta untuk diubah ukurannya dari path.
2. Nama file dasar tersebut disalin ke buffer `base_filename` untuk memastikan keamanan string dan penanganan basename yang dapat mengubah input aslinya.
3. Fungsi membentuk jalur lengkap ke fragmen pertama file ini (`.000`) di direktori `relics`.
4. Fungsi `truncate()` dipanggil pada fragmen fisik `.000` tersebut untuk mengubah ukurannya menjadi size yang diminta.
5. Jika `truncate()` mengembalikan -1, berarti ada kesalahan dalam mengubah ukuran fragmen dan program mengembalikan kode error berupa `-errno`.
6. Fungsi mencatat log truncate.
7. Memori dibersihkan untuk menghindari terjadinya memory leak.
8. Fungsi mengembalikan 0 jika berhasil.

### H. `baymax_utiments()`
Fungsi ini diguankan untuk mengubah waktu akses dan waktu modifikasi dari sebuah file atau direktori. Untuk kodenya seperti ini.
```
static int baymax_utimens(const char *path, const struct timespec tv[2]) {
    char *filename = basename(strdup(path + 1));
    char fragment_path[PATH_MAX];
    snprintf(fragment_path, sizeof(fragment_path), "%s/%s.000", options.relics_dir, filename);
    int res = utimensat(0, fragment_path, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1) {
        free(filename);
        return -errno;
    }
    log_activity("UTIMENS", filename);
    free(filename);
    return 0;
}
```
Dimana untuk cara kerjanya sebagai berikut.
1. Program mendapatkan nama file dasar yang diminta untuk diubah timestamps-nya.
2. Ia membentuk jalur ke fragmen pertama file tersebut (`.000`) di direktori `relics`.
3. Fungsi utimensat() sistem kemudian dipanggil pada fragmen fisik `.000` tersebut untuk mengubah waktu akses dan modifikasinya.
4. Jika ada kesalahan, program mengembalikan kode error berupa `-errno`.
5. Aktivitas `"UTIMENS"` dicatat ke file log.
6. Memori yang dialokasikan dibebaskan untuk menghindari terjadinya memory leak.
7. Fungsi mengembalikan 0 jika operasi berhasil.

### I. `baymax_write()`
ungsi ini digunakan untuk menulis data ke sebuah file dalam FUSE. Untuk kodenya seperti ini
```
static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char *filename = basename(strdup(path + 1));
    char base_filename[256];
    strncpy(base_filename, filename, sizeof(base_filename) - 1);
    base_filename[sizeof(base_filename) - 1] = '\0';
    char log_details[PATH_MAX * 2] = {0};
    int fragment_count = offset / FRAGMENT_SIZE;
    size_t bytes_written = 0;
    size_t current_offset_in_fragment = offset % FRAGMENT_SIZE;

    while (bytes_written < size) {
        char fragment_name[PATH_MAX];
        snprintf(fragment_name, sizeof(fragment_name), "%s.%03d", base_filename, fragment_count);
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s", options.relics_dir, fragment_name);
        size_t fragment_size = FRAGMENT_SIZE - current_offset_in_fragment;
        if (size - bytes_written < fragment_size) {
            fragment_size = size - bytes_written;
        }
        int fragment_fd = open(fragment_path, O_WRONLY | O_CREAT, 0644);
        if (fragment_fd == -1) {
            free(filename);
            return -errno;
        }
        ssize_t written = pwrite(fragment_fd, buf + bytes_written, fragment_size, current_offset_in_fragment);
        if (written == -1) {
            close(fragment_fd);
            free(filename);
            return -errno;
        }
        close(fragment_fd);
        bytes_written += written;
        current_offset_in_fragment = 0;
        fragment_count++;
    }
    snprintf(log_details, sizeof(log_details), "%s -> %s.000", filename, filename);
    log_activity("WRITE", log_details);
    free(filename);
    return bytes_written;
}
```
Dimana untuk cara kerjanya seperti ini.
1. Program mendapatkan nama file dasar tempat data akan ditulis dari `path`.
2. Fungsi menginisialisasi variabel untuk melacak jumlah byte yang sudah ditulis (`bytes_written`), nomor fragmen yang sedang diproses (`fragment_count`), dan offset di dalam fragmen saat ini (`current_offset_in_fragment`) berdasarkan offset permintaan.
3. Loop utama dimulai dan terus berjalan selama masih ada data yang perlu ditulis.
4. Di setiap iterasi, program menentukan nama fragmen yang relevan (misal: filename.000, filename.001, dst.) dan membangun jalur fisiknya.
5. Ia menghitung ukuran data yang akan ditulis ke fragmen saat ini, memastikan tidak melebihi ukuran fragmen atau total size yang diminta.
6. Fragmen fisik dibuka dengan bendera O_WRONLY (tulis saja) dan O_CREAT (buat jika belum ada); jika gagal, error dikembalikan.
7. Fungsi pwrite() digunakan untuk menulis data dari buf ke fragmen fisik pada current_offset_in_fragment yang tepat, memastikan penulisan dimulai dari posisi yang benar dalam fragmen tersebut.
8. Jika `pwrite()` gagal, error dikembalikan, dan file descriptor ditutup.
9. Setelah penulisan berhasil pada fragmen, file descriptor ditutup, bytes_written diperbarui, current_offset_in_fragment direset ke 0 (karena fragmen berikutnya akan ditulis dari awal), dan fragment_count ditingkatkan.
10. Setelah semua data ditulis, program mencatat aktivitas "WRITE" ke file log, dan membebaskan memori yang dialokasikan.
11. Fungsi mengembalikan jumlah total byte yang berhasil ditulis.

### J. `baymax_release()`
Fungsi ini digunakan untuk menangani saat file ditutup setelah file tersebut dibuka. Untuk kodenya seperti ini.
```
static int baymax_release(const char *path, struct fuse_file_info *fi) {
    char *filename = basename(strdup(path + 1));
    char log_details[PATH_MAX * 3] = {0};
    char source_path[PATH_MAX] = {0};
    char real_mountpoint[PATH_MAX];
    if (realpath(mountpoint_path, real_mountpoint) == NULL) {
        strncpy(real_mountpoint, mountpoint_path, sizeof(real_mountpoint) - 1);
        real_mountpoint[sizeof(real_mountpoint) - 1] = '\0';
    }
    if (snprintf(source_path, sizeof(source_path), "%s/%s", real_mountpoint, filename) >= (int)sizeof(source_path)) {
        strncpy(source_path, real_mountpoint, sizeof(source_path) - 1);
        source_path[sizeof(source_path) - 1] = '\0';
        strncat(source_path, "/", sizeof(source_path) - strlen(source_path) - 1);
        strncat(source_path, filename, sizeof(source_path) - strlen(source_path) - 1);
    }
   
    free(filename);
    return 0;
}
```
Dimana untuk cara kerjanya sebagai berikut.
1. Program mendapatkan nama file dasar yang sedang ditutup dari path.
2. Ia mencoba mendapatkan jalur absolut (realpath) dari mount point FUSE. Jika gagal, ia menggunakan jalur mount point yang disimpan.
3. Program kemudian mencoba membangun jalur lengkap ke file virtual tersebut di mount point (source_path). Ini mencakup penanganan potensi buffer overflow.
4. Pada implementasi yang diberikan, tidak ada operasi signifikan yang dilakukan setelah membangun source_path (seperti pencatatan log "RELEASE" atau penyalinan file ke tujuan yang ditentukan oleh options.copy_dest_dir).
5. Memori yang dialokasikan untuk filename dibebaskan.
6. Fungsi mengembalikan 0 untuk menunjukkan operasi penutupan file berhasil.

### K. `log_activity()`
Fungsi ini digunakan untuk mencatat beberapa fungsi yang berhasil dijalankan. Untuk kodenya seperti ini.
Dimana untuk cara kerjanya sebagai berikut.
```
void log_activity(const char *action, const char *details) {
    time_t now;
    struct tm tm_info;
    char timestamp[64];
    time(&now);
    localtime_r(&now, &tm_info);
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", &tm_info);
    FILE *log = fopen(LOG_FILE, "a");
    if (log == NULL) {
        perror("Failed to open log file");
        return;
    }
    fprintf(log, "%s %s: %s\n", timestamp, action, details);
    fclose(log);
}
```
1. Pertama, fungsi ini mengambil waktu saat ini dan memformatnya menjadi timestamp.
2. Lalu, fungsi membuka file log bernama activity.log dalam mode append, yang berarti tulisan baru akan ditambahkan di akhir file.
3. Jika file gagal dibuka, fungsi akan menampilkan pesan error.
4. Selanjutnya, fungsi mencatat aktivitas dengan menuliskan timestamp, jenis tindakan, dan detailnya ke dalam file log.
5. Terakhir, fungsi menutup file log untuk memastikan semua data tersimpan dengan benar.

### L. `get_fragments()`
Fungsi ini digunakan untuk menemukan semua fragmen file yang terkait dengan sebuah nama file dasar. Fungsi ini bertugas mengumpulkan daftar semua nama fragmen tersebut agar konten file asli dapat dibaca atau dimanipulasi secara keseluruhan. Untuk kodenya seperti ini
```
int get_fragments(const char *filename, char **fragments) {
    DIR *dir;
    struct dirent *ent;
    int count = 0;
    dir = opendir(options.relics_dir);
    if (dir != NULL) {
        while ((ent = readdir(dir)) != NULL && count < MAX_FRAGMENTS) {
            if (strncmp(ent->d_name, filename, strlen(filename)) == 0) {
                char *dot = strrchr(ent->d_name, '.');
                if (dot != NULL && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) {
                    fragments[count] = strdup(ent->d_name);
                    count++;
                }
            }
        }
        closedir(dir);
    }
    return count;
}
```
Dimana untuk cara kerjanya seperti ini.
1. Pertama, fungsi ini membuka direktori relics_dir yang telah ditentukan.
2. Lalu, fungsi membaca setiap entry (file atau direktori) di dalamnya.
3. Untuk setiap entry, fungsi memeriksa apakah nama file cocok dengan nama file yang dicari dan memiliki ekstensi numerik .xxx (misalnya, .000, .001, dst.).
4. Jika cocok, nama fragmen tersebut akan disalin ke dalam array fragments.
5. Proses ini berlanjut hingga semua fragmen ditemukan atau batas MAX_FRAGMENTS tercapai.
6. Terakhir, fungsi mengembalikan jumlah fragmen yang ditemukan.
### M. `static struct fuse_opt option_specs[]`
Fungsi dari `static struct fuse_opt option_specs[]` adalah menentukan opsi-opsi baris perintah yang bisa digunakan oleh filesystem FUSE, seperti direktori relik, direktori tujuan salinan, dan nama file tujuan. Untuk kodenya seperti ini.
```
static struct fuse_opt option_specs[] = {

    OPTION("--relics-dir=%s", relics_dir),

    OPTION("--copy-dest=%s", copy_dest_dir),

    OPTION("--dest-filename=%s", dest_filename),

    FUSE_OPT_END

};
```

### N. Fungsi `find_mountpoint()`
Fungsi ini diguankan untuk mencari mountpoint. Untuk kodenya seperti ini
```
static void find_mountpoint(int argc, char *argv[]) {

    for (int i = argc - 1; i > 0; i--) {

        if (argv[i][0] != '-') {

            strncpy(mountpoint_path, argv[i], sizeof(mountpoint_path) - 1);

            mountpoint_path[sizeof(mountpoint_path) - 1] = '\0';

            size_t len = strlen(mountpoint_path);

            if (len > 1 && mountpoint_path[len - 1] == '/') {

                mountpoint_path[len - 1] = '\0';

            }

            break;

        }

    }

}
```
Dimana untuk cara kerjanya sebagai berikut.
1. Fungsi mencari mountpoint Fungsi ini mencari argumen terakhir di baris perintah yang tidak diawali dengan tanda strip (-).
2. Setelah itu, fungsi menyimpan path Argumen yang ditemukan ini dianggap sebagai mountpoint dan disimpan ke mountpoint_path.
3. Fungsi menghilangkan garis miring Jika path berakhir dengan garis miring (/), garis miring itu dihilangkan untuk normalisasi.
4. Terakhir, fungsi mengakhiri pencarian Setelah mountpoint ditemukan, proses pencarian dihentikan.

### O. `int_main()`
Untuk int_main sendiri berisi inisialisasi, fing_mountpoint, fuse_operation, fungsi dari FUSE itu sendiri, dan pembersihan memori agar tidak terjadi memory leak. Untuk kodenya seperti ini.
```
int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    options.relics_dir = strdup("relics");
    options.copy_dest_dir = NULL;
    options.dest_filename = NULL;
    if (fuse_opt_parse(&args, &options, option_specs, NULL) == -1)
        return 1;
    find_mountpoint(args.argc, args.argv);
    mkdir(options.relics_dir, 0777);

    static struct fuse_operations baymax_oper = {
        .getattr    = baymax_getattr,
        .readdir    = baymax_readdir,
        .open       = baymax_open,
        .read       = baymax_read,
        .unlink     = baymax_unlink,
        .create     = baymax_create,
        .truncate   = baymax_truncate,
        .utimens    = baymax_utimens,
        .write      = baymax_write,
        .release = baymax_release
    };

    int ret = fuse_main(args.argc, args.argv, &baymax_oper, NULL);

    free(options.relics_dir);
    if (options.copy_dest_dir)
        free(options.copy_dest_dir);
    if (options.dest_filename)
        free(options.dest_filename);

    return ret;
}
```
Untuk cara kerjanya sebagai berikut.
1. Pertama, program menyiapkan argumen dan opsi FUSE dari baris perintah.
2. Lalu, program menentukan titik mount dan memastikan direktori "relics" ada.
3. Setelah itu, program mendefinisikan cara filesystem akan berinteraksi (baca, tulis, dan sebagainya).
4. Kemudian, program menjalankan FUSE, membuat filesystem virtual berfungsi.
5. Terakhir, program membersihkan memori setelah sistem file tidak lagi digunakan.


## soal_3
## soal_4 
