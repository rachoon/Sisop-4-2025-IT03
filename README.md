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

pada soal kali ini kita diminta untuk membuat sebuah program yang dapat mendeteksi file berbahaya kemudian membalikan nama file tersebut  lalu mencetak log serta mengenkriipsi file yang tidak berbahaya dengan ROT13

<h3>
    antink.c
</h3>

```c
#define FUSE_USE_VERSION 30
```
Mengaktifkan penggunaan FUSE versi 3.0 (FUSE_USE_VERSION 30)

```c
void rot13(char *str) {
    for (; *str; ++str) {
        if ((*str >= 'a' && *str <= 'm') || (*str >= 'A' && *str <= 'M')) {
            *str += 13;
        } else if ((*str >= 'n' && *str <= 'z') || (*str >= 'N' && *str <= 'Z')) {
            *str -= 13;
        }
    }
}
```
<li> Mengubah huruf alfabet menjadi bentuk ROT13 (menggeser 13 karakter) </li>
<li> hanya berlaku untuk file non-berbahaya saat dibaca. </li>

```c
void reverse_string(char *str) {
    if (!str) return;
    int i = 0, j = strlen(str) - 1;
    while (i < j) {
        char c = str[i];
        str[i++] = str[j];
        str[j--] = c;
    }
}
```
Membalikkan isi string, digunakan untuk menampilkan nama file berbahaya dalam kondisi terbalik.

```c
int is_dangerous(const char *filename) {
    return strstr(filename, KEYWORD1) || strstr(filename, KEYWORD2);
}
```
Mengembalikan nilai true jika nama file mengandung "nafis" atau "kimcun".

```c
void log_activity(const char *filename, const char *action) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] %s: %s\n", ctime(&now), action, filename);
        fclose(log);
    }
}
```
Membuka file log /var/log/it24.log lalu menuliskan waktu, aksi (DANGEROUS_FILE), dan nama file.

getattr
```c
static int antink_getattr(const char *path, struct stat *st) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "/it24_host%s", path);
    return lstat(full_path, st) ? -errno : 0;
}

```
<li>Menyediakan metadata file seperti ukuran, mode, dll.</li>
<li>File asli dicari di /it24_host.</li>

readdir
```c
static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                         off_t offset, struct fuse_file_info *fi) {
    DIR *dp;
    struct dirent *de;
    char full_path[MAX_PATH_LEN];
    
    snprintf(full_path, sizeof(full_path), "/it24_host%s", path);
    if (!(dp = opendir(full_path))) return -errno;

    while ((de = readdir(dp))) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char disp_name[MAX_PATH_LEN];
        strcpy(disp_name, de->d_name);

        if (is_dangerous(de->d_name)) {
            reverse_string(disp_name);
            log_activity(de->d_name, "DANGEROUS_FILE");
        }

        if (filler(buf, disp_name, &st, 0)) break;
    }
    closedir(dp);
    return 0;
}
```

<li>Menyediakan metadata file seperti ukuran, mode, dll.</li>
<li>File asli dicari di /it24_host.</li>
<li>Membuka direktori /it24_host/<path>.</li>
<li>Untuk setiap entry file/direktori:<br>
Disiapkan info stat-nya <br>
Nama file disalin ke buffer disp_name</li>
<li>Jika berbahaya:
<br>
Nama file dibalik
<br>
Dicatat ke log</li>
<li>Mengisi hasil isi direktori yang ditampilkan ke user</li>

Struktur fuse_operations
```c
static struct fuse_operations antink_ops = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open = antink_open,
    .read = antink_read,
};
```
Struktur fuse_operations adalah tabel fungsi callback yang memberitahu FUSE bagaimana menangani operasi sistem file tertentu.
Setiap entri di struktur ini mengarah ke fungsi handler yang telah kita implementasikan.

<table border="1">
  <thead>
    <tr>
      <th>Baris</th>
      <th>Makna</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><code>.getattr = antink_getattr,</code></td>
      <td>Digunakan untuk menangani permintaan metadata file, misalnya saat user menjalankan <code>ls -l</code> atau <code>stat</code>. FUSE akan memanggil <code>antink_getattr()</code> untuk mengisi informasi seperti ukuran file, permission, dll.</td>
    </tr>
    <tr>
      <td><code>.readdir = antink_readdir,</code></td>
      <td>Digunakan saat user ingin melihat isi direktori, misalnya dengan <code>ls</code>. Fungsi <code>antink_readdir()</code> akan memberikan daftar nama file dan folder di direktori tersebut.</td>
    </tr>
    <tr>
      <td><code>.open = antink_open,</code></td>
      <td>Digunakan saat user mencoba membuka file. Fungsi <code>antink_open()</code> memverifikasi bahwa file bisa dibuka, dan menyiapkan file descriptor.</td>
    </tr>
    <tr>
      <td><code>.read = antink_read,</code></td>
      <td>Digunakan saat user membaca file. Fungsi <code>antink_read()</code> akan membaca isi file dari sistem file backend dan memberikan hasilnya (yang bisa dimodifikasi, seperti ROT13).</td>
    </tr>
  </tbody>
</table>

Fungsi main()

```c
int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &antink_ops, NULL);
}
```
Fungsi dan Tujuan:
Fungsi main() adalah titik awal program seperti biasa. Di dalamnya kita memanggil fuse_main() yang akan:
<br>
Menyambungkan sistem file kita ke mount point
<br>
Menangani request dari user (melalui kernel)
<br>
Menjalankan loop yang menunggu dan merespon operasi sistem file.

<h3>
    Dockerfile
</h3>

```dockerfile
FROM ubuntu:22.04
```
Menggunakan image dasar Ubuntu versi 22.04.
<br>
Ini adalah sistem operasi minimal yang cocok untuk membangun dan menjalankan aplikasi berbasis FUSE (Filesystem in Userspace).

```dockerfile
RUN apt-get update && \
    apt-get install -y \
    fuse \
    libfuse-dev \
    gcc \
    make \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*
```
<ul>
  <li> <code>apt-get update</code>: Memperbarui daftar paket.</li>
  <li> <code>apt-get install -y</code>: Menginstal beberapa dependensi penting:
    <ul>
      <li><code>fuse</code>: Binary utama untuk menjalankan filesystem FUSE.</li>
      <li><code>libfuse-dev</code>: Header dan library untuk mengembangkan aplikasi berbasis FUSE.</li>
      <li><code>gcc</code>: Compiler C yang digunakan untuk menyusun program <code>antink.c</code>.</li>
      <li><code>make</code>: Utilitas build system (meskipun tidak digunakan langsung di Dockerfile ini).</li>
      <li><code>pkg-config</code>: Digunakan untuk mencari informasi build library (bisa membantu saat build proyek yang kompleks).</li>
    </ul>
  </li>
  <li> <code>rm -rf /var/lib/apt/lists/*</code>: Membersihkan cache apt untuk mengurangi ukuran image Docker.</li>
</ul>

```dockerfile
WORKDIR /app
COPY antink.c .
RUN gcc antink.c -o antink -D_FILE_OFFSET_BITS=64 -lfuse
HEALTHCHECK --interval=5s --timeout=3s --retries=3 \
  CMD ls /antink_mount && [ -f /var/log/it24.log ]
```
Menetapkan direktori kerja menjadi /app.
Semua perintah selanjutnya akan dijalankan dari folder ini.

<br>

Menyalin file sumber antink.c dari direktori lokal ke folder /app di dalam container.
<br>
Menyusun program antink.c menggunakan gcc.
<br>
-o antink: Outputnya adalah file binary bernama antink.
<br>
-D_FILE_OFFSET_BITS=64: Flag penting agar filesystem bisa menangani file besar (>2GB).
<br>
-lfuse: Melink ke library FUSE. Catatan: karena image Ubuntu ini menggunakan libfuse2, kita pakai -lfuse, bukan -lfuse3.


```dockerfile
HEALTHCHECK --interval=5s --timeout=3s --retries=3 \
  CMD ls /antink_mount && [ -f /var/log/it24.log ]
CMD ["/app/antink", "-f", "/antink_mount"]
```

Penambahan health check pada container bertujuan untuk memastikan bahwa sistem berjalan dengan benar. Container dianggap sehat jika direktori mount /antink_mount dapat diakses, yang menandakan bahwa sistem FUSE aktif, dan jika file log /var/log/it24.log telah dibuat, menunjukkan bahwa program utama sudah berjalan dan melakukan logging. Pemeriksaan dilakukan secara berkala setiap 5 detik, dengan batas waktu (timeout) 3 detik, dan akan dicoba hingga 3 kali sebelum container dinyatakan gagal. Saat container dimulai, perintah utama yang dijalankan adalah menjalankan program antink dengan flag -f, yang memastikan program berjalan di foreground mode—hal ini penting dalam konteks Docker agar proses utama tidak berjalan sebagai daemon. Direktori /antink_mount berfungsi sebagai mount point FUSE dan harus dipastikan sudah tersedia di host atau di dalam container saat eksekusi.

<h3>docker-compose.yml </h3>
File ini mendefinisikan dua container layanan: antink-server dan antink-logger. Container ini bekerja bersama untuk menjalankan filesystem berbasis FUSE (antink) dan mengakses log-nya secara terpisah.

```yml
version: '3.8'
```
Menentukan versi docker-compose schema yang digunakan (3.8) — stabil dan kompatibel dengan Docker Desktop dan Docker Engine terbaru.

```yml
services:
  antink-server:
    build: .
    container_name: antink-server
    privileged: true
    volumes:
      - ./it24_host:/it24_host
      - ./antink_mount:/antink_mount:rshared
      - ./antink-logs:/var/log
```
Membangun image dari Dockerfile yang berada di direktori saat ini (.).
<br>
Menetapkan nama container menjadi antink-server.
<br>
Mengaktifkan akses istimewa penuh ke host. Wajib untuk filesystem FUSE agar bisa mount/akses /dev/fuse.
<br>
Mount direktori lokal ke dalam container:
<ul>
  <li><code>./it24_host</code>: Sumber file nyata di host.</li>
  <li><code>./antink_mount</code>: Tempat filesystem FUSE dimount oleh <code>antink</code>.</li>
  <li><code>:rshared</code>: Opsi <em>mount propagation</em> agar perubahan mount terlihat oleh host atau container lain. Penting untuk FUSE agar berfungsi dengan benar.</li>
  <li><code>./antink-logs</code>: Folder di host untuk menyimpan file log dari container, yaitu <code>/var/log/it24.log</code>.</li>
</ul>

```yml
    cap_add:
      - SYS_ADMIN
    devices:
      - /dev/fuse
    restart: unless-stopped
  antink-logger:
    image: alpine:latest
    container_name: antink-logger
    volumes:
      - ./antink-logs:/var/log
    depends_on:
      antink-server:
        condition: service_healthy
    restart: unless-stopped
```

Bagian ini mengatur konfigurasi lanjutan untuk dua layanan. Pada antink-server, opsi cap_add: - SYS_ADMIN memberi hak administratif ke container agar dapat melakukan operasi mount FUSE, sementara devices: - /dev/fuse memberi akses langsung ke perangkat FUSE di host. Opsi restart: unless-stopped memastikan container akan otomatis dimulai ulang kecuali dihentikan secara manual. Layanan antink-logger menggunakan image ringan alpine:latest, dan diberi akses ke volume log yang sama (./antink-logs:/var/log) agar dapat membaca log aktivitas dari antink-server. depends_on dengan condition: service_healthy memastikan antink-logger hanya berjalan setelah antink-server dipastikan sehat oleh healthcheck, menjaga urutan inisialisasi layanan dengan benar.
## soal_4 
Pada soal ini kita mendapatkan pekerjaan di SEGA sebagai char designer untuk game maimai dan dipromosikan menjadi administrator, lalu kita diberi tugas untuk memastikan 7 area di maimai berfungsi seperti semestinya.<br>
1. Starter Chiho sebagai menyimpan file yang masuk secara normal.<br>
2. Metropolis Chiho sebagai file directory asli di shift sesuai lokasinya.<br>
3. Dragon Chiho sebagai File yang disimpan di area ini dienkripsi menggunakan ROT13 di direktori asli. <br>
4. Black Rose Chiho untuk menyimpan data dalam format biner murni, tanpa enkripsi atau encoding tambahan.
5. Heaven Chiho untuk melindungi file dengan nkripsi AES-256-CBC.<br>
6. Skystreet Chiho untuk dikompresi menggunakan gzip untuk menghemat storage.<br>
7. 7sRef Chiho adalah area spesial yang dapat mengakses semua area lain melalui sistem penamaan khusus.



```c
static const char *CHIHO_BASE_DIR = "chiho"; 
static char REAL_CHIHO_BASE_PATH[PATH_MAX];
```

memiliki fungsi sebagai penyimpan informasi direktori dasar (base directory) dan path absolut dari direktori tersebut dalam sistem C, yang biasanya digunakan dalam program seperti FUSE filesystem atau program yang mengelola struktur direktori khusus.

```c
static const unsigned char g_aes_key[32] = "0123456789abcdef0123456789abcdef";
#define AES_BLOCK_SIZE 16
```

Kode tersebut berfungsi untuk Mendefinisikan kunci enkripsi/dekripsi berukuran 256-bit (32 byte) untuk digunakan dalam algoritma AES-256.

```c
typedef enum {
    TYPE_STARTER,
    TYPE_METRO,
    TYPE_DRAGON,
    TYPE_BLACKROSE,
    TYPE_HEAVEN,
    TYPE_YOUTH,
    TYPE_7SREF,
    TYPE_ROOT,
    TYPE_UNKNOWN
} ChihoType;
```

Enumerasi yang mendefinisikan berbagai jenis "chiho" (area/direktori) dalam sistem universe maimai (seperti yang sedang kamu bangun dengan FUSE).

```c
ChihoType get_chiho_type_and_real_path(const char *path, char *real_path_buf, char *original_filename_buf) {
    char temp_path[PATH_MAX];
    strncpy(temp_path, path, PATH_MAX -1); // Salin path agar bisa dimodifikasi
    temp_path[PATH_MAX-1] = '\0';

    if (original_filename_buf) original_filename_buf[0] = '\0';

    if (strcmp(path, "/") == 0) {
        strncpy(real_path_buf, REAL_CHIHO_BASE_PATH, PATH_MAX -1);
        real_path_buf[PATH_MAX-1] = '\0';
        return TYPE_ROOT;
    }

    char *first_slash = strchr(temp_path + 1, '/');
    char chiho_name[256] = {0};
    char filename_component[PATH_MAX] = {0}; 

    if (first_slash) {
        size_t chiho_len_val = first_slash - (temp_path + 1);
        if (chiho_len_val >= sizeof(chiho_name)) {
            chiho_len_val = sizeof(chiho_name) - 1;
        }
        strncpy(chiho_name, temp_path + 1, chiho_len_val);
        chiho_name[chiho_len_val] = '\0';

        strncpy(filename_component, first_slash + 1, sizeof(filename_component) - 1);

        if (original_filename_buf) {
            strncpy(original_filename_buf, filename_component, PATH_MAX - 1); 
            original_filename_buf[PATH_MAX - 1] = '\0';
        }
    } else {
        strncpy(chiho_name, temp_path + 1, sizeof(chiho_name) -1);
    }

    ChihoType type = TYPE_UNKNOWN;
    size_t len_base = strlen(REAL_CHIHO_BASE_PATH);
    size_t len_filename_comp = strlen(filename_component);

    if (strcmp(chiho_name, "starter") == 0) {
        type = TYPE_STARTER;
        if (strlen(filename_component) > 0) {
            if (len_base + 1 + 7 + 1 + len_filename_comp + 4 + 1 > PATH_MAX) {
                real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/starter/%s.mai", REAL_CHIHO_BASE_PATH, filename_component);
            }
        } else {
            if (len_base + 1 + 7 + 1 > PATH_MAX) {
                 real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/starter", REAL_CHIHO_BASE_PATH);
            }
        }
    } else if (strcmp(chiho_name, "metro") == 0) {
        type = TYPE_METRO;
        if (strlen(filename_component) > 0) {
            if (len_base + 1 + 5 + 1 + len_filename_comp + 1 > PATH_MAX) {
                real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/metro/%s", REAL_CHIHO_BASE_PATH, filename_component);
            }
        } else {
             if (len_base + 1 + 5 + 1 > PATH_MAX) {
                 real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/metro", REAL_CHIHO_BASE_PATH);
            }
        }
    } else if (strcmp(chiho_name, "dragon") == 0) {
        type = TYPE_DRAGON;
         if (strlen(filename_component) > 0) {
            if (len_base + 1 + 6 + 1 + len_filename_comp + 1 > PATH_MAX) {
                real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/dragon/%s", REAL_CHIHO_BASE_PATH, filename_component);
            }
        } else {
            if (len_base + 1 + 6 + 1 > PATH_MAX) {
                 real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/dragon", REAL_CHIHO_BASE_PATH);
            }
        }
    } else if (strcmp(chiho_name, "blackrose") == 0) {
        type = TYPE_BLACKROSE;
        if (strlen(filename_component) > 0) {
            if (len_base + 1 + 9 + 1 + len_filename_comp + 1 > PATH_MAX) {
                real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/blackrose/%s", REAL_CHIHO_BASE_PATH, filename_component);
            }
        } else {
            if (len_base + 1 + 9 + 1 > PATH_MAX) {
                 real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/blackrose", REAL_CHIHO_BASE_PATH);
            }
        }
    } else if (strcmp(chiho_name, "heaven") == 0) {
        type = TYPE_HEAVEN;
        if (strlen(filename_component) > 0) {
            if (len_base + 1 + 6 + 1 + len_filename_comp + 1 > PATH_MAX) {
                real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/heaven/%s", REAL_CHIHO_BASE_PATH, filename_component);
            }
        } else {
            if (len_base + 1 + 6 + 1 > PATH_MAX) {
                 real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/heaven", REAL_CHIHO_BASE_PATH);
            }
        }
    } else if (strcmp(chiho_name, "youth") == 0) {
        type = TYPE_YOUTH;
        if (strlen(filename_component) > 0) {
            if (len_base + 1 + 5 + 1 + len_filename_comp + 1 > PATH_MAX) {
                real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/youth/%s", REAL_CHIHO_BASE_PATH, filename_component);
            }
        } else {
             if (len_base + 1 + 5 + 1 > PATH_MAX) {
                 real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
            } else {
                snprintf(real_path_buf, PATH_MAX, "%s/youth", REAL_CHIHO_BASE_PATH);
            }
        }
    } else if (strcmp(chiho_name, "7sref") == 0) {
        type = TYPE_7SREF;
        if (strlen(filename_component) > 0) {
             char *underscore = strchr(filename_component, '_');
             if (underscore) {
                char area_prefix[256]; 
                size_t area_prefix_len = underscore - filename_component;
                if (area_prefix_len >= sizeof(area_prefix)) area_prefix_len = sizeof(area_prefix) -1;
                strncpy(area_prefix, filename_component, area_prefix_len);
                area_prefix[area_prefix_len] = '\0';
                
                char actual_filename_comp[PATH_MAX] = {0};
                strncpy(actual_filename_comp, underscore + 1, sizeof(actual_filename_comp) - 1);

                if (original_filename_buf) {
                    strncpy(original_filename_buf, actual_filename_comp, PATH_MAX - 1);
                    original_filename_buf[PATH_MAX - 1] = '\0';
                }
                
                size_t len_actual_filename_comp = strlen(actual_filename_comp);

                if (strcmp(area_prefix, "starter") == 0) {
                    if (len_base + 1 + 7 + 1 + len_actual_filename_comp + 4 + 1 > PATH_MAX) { 
                        real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
                    } else {
                        snprintf(real_path_buf, PATH_MAX, "%s/starter/%s.mai", REAL_CHIHO_BASE_PATH, actual_filename_comp);
                    }
                } else if (strcmp(area_prefix, "metro") == 0) {
                     if (len_base + 1 + 5 + 1 + len_actual_filename_comp + 1 > PATH_MAX) { 
                        real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
                    } else {
                        snprintf(real_path_buf, PATH_MAX, "%s/metro/%s", REAL_CHIHO_BASE_PATH, actual_filename_comp);
                    }
                } else if (strcmp(area_prefix, "dragon") == 0) {
                     if (len_base + 1 + 6 + 1 + len_actual_filename_comp + 1 > PATH_MAX) { 
                        real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
                    } else {
                        snprintf(real_path_buf, PATH_MAX, "%s/dragon/%s", REAL_CHIHO_BASE_PATH, actual_filename_comp);
                    }
                } else if (strcmp(area_prefix, "blackrose") == 0) {
                    if (len_base + 1 + 9 + 1 + len_actual_filename_comp + 1 > PATH_MAX) { 
                        real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
                    } else {
                        snprintf(real_path_buf, PATH_MAX, "%s/blackrose/%s", REAL_CHIHO_BASE_PATH, actual_filename_comp);
                    }
                } else if (strcmp(area_prefix, "heaven") == 0) {
                     if (len_base + 1 + 6 + 1 + len_actual_filename_comp + 1 > PATH_MAX) { 
                        real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
                    } else {
                        snprintf(real_path_buf, PATH_MAX, "%s/heaven/%s", REAL_CHIHO_BASE_PATH, actual_filename_comp);
                    }
                } else if (strcmp(area_prefix, "youth") == 0) {
                     if (len_base + 1 + 5 + 1 + len_actual_filename_comp + 1 > PATH_MAX) { 
                        real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
                    } else {
                        snprintf(real_path_buf, PATH_MAX, "%s/youth/%s", REAL_CHIHO_BASE_PATH, actual_filename_comp);
                    }
                } else {
                    type = TYPE_UNKNOWN;
                    real_path_buf[0] = '\0';
                }
             } else {
                type = TYPE_UNKNOWN;
                real_path_buf[0] = '\0';
             }
        } else {
            strncpy(real_path_buf, REAL_CHIHO_BASE_PATH, PATH_MAX-1); 
            real_path_buf[PATH_MAX-1] = '\0';
        }
    } else {
        if (len_base + 1 + strlen(chiho_name) + 1 > PATH_MAX) {
            real_path_buf[0] = '\0'; type = TYPE_UNKNOWN;
        } else {
            snprintf(real_path_buf, PATH_MAX, "%s/%s", REAL_CHIHO_BASE_PATH, chiho_name);
        }
    }
    return type;
}
```

Kode tersebut berfungsi untuk mendeteksi tipe area(chiho) berdasarkan path pengguna, menghasilkan path sebenarnya, dan menyimpan nama file asli.

```c
void metro_transform(char *data, size_t len, int direction) { 
    for (size_t i = 0; i < len; i++) {
        data[i] = (unsigned char)data[i] + (direction * (i % 256));
    }
}
```

Kode tersebut berfungsi untuk melakukan transformasi (encoding atau decoding) pada sebuah buffer data.

```c
void rot13_transform(char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if ((str[i] >= 'A' && str[i] <= 'M') || (str[i] >= 'a' && str[i] <= 'm')) {
            str[i] += 13;
        } else if ((str[i] >= 'N' && str[i] <= 'Z') || (str[i] >= 'n' && str[i] <= 'z')) {
            str[i] -= 13;
        }
    }
}
```

Kode tersebut digunakan untuk melakukan ROT13 cipher pada string yang diberikan.

```c
int aes_crypt(const unsigned char *input, int input_len,
              unsigned char *output, int *output_len,
              int do_encrypt, unsigned char *iv_in_out) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len = 0; 
    int plaintext_len = 0;  

    if(!(ctx = EVP_CIPHER_CTX_new())) return -1; 

    if(1 != EVP_CipherInit_ex(ctx, EVP_aes_256_cbc(), NULL, g_aes_key, iv_in_out, do_encrypt)) {
        EVP_CIPHER_CTX_free(ctx);
        return -1; 
    }

    if(1 != EVP_CipherUpdate(ctx, output, &len, input, input_len)) {
        EVP_CIPHER_CTX_free(ctx);
        return -1; 
    }
    if (do_encrypt) ciphertext_len = len; else plaintext_len = len;

    if(1 != EVP_CipherFinal_ex(ctx, output + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return -1; 
    }
    if (do_encrypt) ciphertext_len += len; else plaintext_len += len;

    *output_len = do_encrypt ? ciphertext_len : plaintext_len;

    EVP_CIPHER_CTX_free(ctx);
    return 0; 
}
```
Fungsi dari kode tersebut adalah implementasi enkripsi dan dekripsi AES-256-CBC menggunakan OpenSSL EVP API.

```c
int gzip_compress_data(const char *src, size_t src_len, char **dst, size_t *dst_len) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return -1; 
    }
    zs.next_in = (Bytef*)src;
    zs.avail_in = src_len;

    size_t out_buf_size = deflateBound(&zs, src_len);
    *dst = (char*)malloc(out_buf_size);
    if (!*dst) {
        deflateEnd(&zs);
        return -ENOMEM;
    }

    zs.next_out = (Bytef*)*dst;
    zs.avail_out = out_buf_size;

    int ret = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        free(*dst);
        *dst = NULL;
        return -1; 
    }
    *dst_len = zs.total_out;
    return 0; 
}
```
Kode tersebut bertujuan untuk mengompresi data yang diberikan menggunakan format gzip dan menyimpan hasilnya di buffer yang dialokasikan secara dinamis.

```c
int gzip_decompress_data(const char *src, size_t src_len, char **dst, size_t *dst_len) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 15 + 16) != Z_OK) { 
        return -1; 
    }
    zs.next_in = (Bytef*)src;
    zs.avail_in = src_len;

    size_t out_buf_size_increment = src_len * 2 > 1024 ? src_len * 2 : 1024; 
    size_t current_buf_size = 0;
    *dst = NULL;
    *dst_len = 0;

    int ret;
    do {
        current_buf_size += out_buf_size_increment;
        char *new_dst = (char*)realloc(*dst, current_buf_size);
        if (!new_dst) {
            free(*dst);
            *dst = NULL;
            inflateEnd(&zs);
            return -ENOMEM;
        }
        *dst = new_dst;

        zs.next_out = (Bytef*)(*dst + *dst_len);
        zs.avail_out = current_buf_size - *dst_len;
        
        ret = inflate(&zs, Z_NO_FLUSH);
        *dst_len = zs.total_out;

        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            free(*dst);
            *dst = NULL;
            inflateEnd(&zs);
            return -1; 
        }
    } while (ret != Z_STREAM_END && zs.avail_out == 0); 

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        free(*dst);
        *dst = NULL;
        return -1; 
    }
    return 0; 
}
```

Kode tersebut berfungsi untuk mendekompresi data yang sudah dikompresi dengan format gzip dan menyimpan hasil dekompresi di buffer yang dialokasikan secara dinamis.

```c
static int maimai_getattr(const char *path, struct stat *stbuf) {
    char real_path[PATH_MAX];
    ChihoType type = get_chiho_type_and_real_path(path, real_path, NULL);

    memset(stbuf, 0, sizeof(struct stat));

    if (type == TYPE_UNKNOWN && strlen(real_path) == 0) { 
        return -ENOENT;
    }
    if (type == TYPE_ROOT || (type == TYPE_7SREF && strchr(path, '_') == NULL && strcmp(path, "/7sref") == 0 )) { 
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2; 
        if (type == TYPE_ROOT) {
             if (lstat(REAL_CHIHO_BASE_PATH, stbuf) == -1) return -errno;
        }
        return 0;
    }
    
    if (strchr(path + 1, '/') == NULL && type != TYPE_7SREF) { 
        if (lstat(real_path, stbuf) == -1) {
            return -errno;
        }
        return 0;
    }

    int res = lstat(real_path, stbuf);
    if (res == -1) return -errno;

    if (type == TYPE_HEAVEN && S_ISREG(stbuf->st_mode)) {
        if (stbuf->st_size >= AES_BLOCK_SIZE) { 
            stbuf->st_size -= AES_BLOCK_SIZE;
            if (stbuf->st_size < 0) stbuf->st_size = 0;
        } else {
            stbuf->st_size = 0; 
        }
    }
    return 0;
}
```

Fungsi dari kode tersebut adalah implementasi handler untuk operasi getattr di sebuah sistem filesystem (kemungkinan FUSE) yang mengembalikan atribut (stat) file atau direktori berdasarkan path virtual yang diberikan.

```c
static int maimai_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) { 
    (void) offset; 
    (void) fi; 

    char real_path[PATH_MAX];
    char original_filename[PATH_MAX]; 
    ChihoType type = get_chiho_type_and_real_path(path, real_path, original_filename);

    filler(buf, ".", NULL, 0); 
    filler(buf, "..", NULL, 0);

    if (type == TYPE_ROOT) {
        const char *chihos[] = {"starter", "metro", "dragon", "blackrose", "heaven", "youth", "7sref"};
        for (int i = 0; i < 7; i++) {
            filler(buf, chihos[i], NULL, 0); 
        }
        return 0;
    }
    
    if (type == TYPE_7SREF && (strcmp(path, "/7sref") == 0 || strcmp(path, "/7sref/") == 0)) {
        const char *source_chihos[] = {"starter", "metro", "dragon", "blackrose", "heaven", "youth"};
        const ChihoType source_types[] = {TYPE_STARTER, TYPE_METRO, TYPE_DRAGON, TYPE_BLACKROSE, TYPE_HEAVEN, TYPE_YOUTH};
        size_t len_base = strlen(REAL_CHIHO_BASE_PATH);

        for (int i = 0; i < 6; i++) {
            char source_chiho_path[PATH_MAX];
            size_t len_source_chiho_name = strlen(source_chihos[i]);
            if (len_base + 1 + len_source_chiho_name + 1 > PATH_MAX) {
                fprintf(stderr, "Warning: Path too long for constructing %s/%s in readdir for 7sref.\n", REAL_CHIHO_BASE_PATH, source_chihos[i]);
                continue; 
            }
            snprintf(source_chiho_path, PATH_MAX, "%s/%s", REAL_CHIHO_BASE_PATH, source_chihos[i]);
            
            DIR *dp = opendir(source_chiho_path);
            if (dp == NULL) continue; 

            struct dirent *de;
            while ((de = readdir(dp)) != NULL) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

                char sref_name[PATH_MAX];
                char actual_name_val[NAME_MAX + 1]; 
                
                strncpy(actual_name_val, de->d_name, NAME_MAX);
                actual_name_val[NAME_MAX] = '\0';


                if (source_types[i] == TYPE_STARTER) {
                    char *dot_mai = strstr(actual_name_val, ".mai");
                    if (dot_mai && strlen(dot_mai) == 4) { 
                        *dot_mai = '\0';
                    } else {
                        continue; 
                    }
                }
                snprintf(sref_name, PATH_MAX, "%s_%s", source_chihos[i], actual_name_val);
                filler(buf, sref_name, NULL, 0); 
            }
            closedir(dp);
        }
        return 0;
    }

    DIR *dp = opendir(real_path); 
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        
        char name_to_fill[NAME_MAX + 1]; // Use NAME_MAX for safety
        strncpy(name_to_fill, de->d_name, NAME_MAX);
        name_to_fill[NAME_MAX] = '\0';


        if (type == TYPE_STARTER) {
            char *dot_mai = strstr(name_to_fill, ".mai");
            if (dot_mai && strlen(dot_mai) == 4) { 
                *dot_mai = '\0';
            } else {
                 continue; 
            }
        }
        filler(buf, name_to_fill, NULL, 0); 
    }
    closedir(dp);
    return 0;
}
```

Fungsi dari kode ini adalah implementasi handler untuk operasi readdir di filesystem berbasis FUSE, yang bertugas membaca isi direktori virtual dan mengisi daftar isi direktori yang diminta (path), lalu mengirimkan entri-entri tersebut ke kernel/klien melalui callback filler.

```c
static int maimai_open(const char *path, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    ChihoType type = get_chiho_type_and_real_path(path, real_path, NULL);

    if (type == TYPE_UNKNOWN && strlen(real_path) == 0) return -ENOENT;
    if (type == TYPE_ROOT || (type == TYPE_7SREF && strchr(path, '_') == NULL)) return -EISDIR;
    if (real_path[0] == '\0') return -ENOENT;


    int res = open(real_path, fi->flags);
    if (res == -1) return -errno;
    close(res); 
    return 0;
}
```

Fungsi dari kode tersebut adalah implementasi handler untuk operasi open di FUSE filesystem yang kamu buat.

```c
static int maimai_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void)fi;
    char real_path[PATH_MAX];
    char original_filename[PATH_MAX]; 
    ChihoType type_of_fuse_path = get_chiho_type_and_real_path(path, real_path, original_filename);
    
    if (real_path[0] == '\0') return -ENOENT;

    ChihoType effective_type = type_of_fuse_path;
    if (type_of_fuse_path == TYPE_7SREF) {
        if (strstr(real_path, "/starter/")) effective_type = TYPE_STARTER;
        else if (strstr(real_path, "/metro/")) effective_type = TYPE_METRO;
        else if (strstr(real_path, "/dragon/")) effective_type = TYPE_DRAGON;
        else if (strstr(real_path, "/blackrose/")) effective_type = TYPE_BLACKROSE;
        else if (strstr(real_path, "/heaven/")) effective_type = TYPE_HEAVEN;
        else if (strstr(real_path, "/youth/")) effective_type = TYPE_YOUTH;
        else return -ENOENT; 
    }

    if (effective_type == TYPE_UNKNOWN || (type_of_fuse_path == TYPE_UNKNOWN && strlen(real_path) == 0)) return -ENOENT;
    if (effective_type == TYPE_ROOT) return -EISDIR;

    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return -errno;
    }
    size_t file_size = st.st_size;
    if (file_size == 0 && effective_type != TYPE_HEAVEN) {
         close(fd);
         return 0; 
    }

    char *file_content = (char *)malloc(file_size + 1); 
    if (!file_content) {
        close(fd);
        return -ENOMEM;
    }

    ssize_t read_bytes = pread(fd, file_content, file_size, 0); 
    close(fd);

    if (read_bytes == -1) {
        free(file_content);
        return -errno;
    }
    file_content[read_bytes] = '\0'; 

    char *processed_content = NULL;
    size_t processed_size = 0;
    int res_transform = 0;

    switch (effective_type) {
        case TYPE_STARTER: 
        case TYPE_BLACKROSE:
            processed_content = file_content; 
            processed_size = read_bytes;
            file_content = NULL; 
            break;
        case TYPE_METRO:
            processed_content = file_content; 
            metro_transform(processed_content, read_bytes, -1); 
            processed_size = read_bytes;
            file_content = NULL;
            break;
        case TYPE_DRAGON:
            processed_content = file_content; 
            rot13_transform(processed_content, read_bytes); 
            processed_size = read_bytes;
            file_content = NULL;
            break;
        case TYPE_HEAVEN:
            if (read_bytes < AES_BLOCK_SIZE) { 
                free(file_content);
                return 0; 
            }
            unsigned char iv[AES_BLOCK_SIZE];
            memcpy(iv, file_content, AES_BLOCK_SIZE);
            
            processed_content = (char*)malloc(read_bytes); 
            if (!processed_content) {
                free(file_content);
                return -ENOMEM;
            }
            int decrypted_len;
            res_transform = aes_crypt((unsigned char*)file_content + AES_BLOCK_SIZE, read_bytes - AES_BLOCK_SIZE,
                            (unsigned char*)processed_content, &decrypted_len, 0, iv);
            free(file_content);
            if (res_transform != 0) {
                free(processed_content);
                return -EIO; 
            }
            processed_size = decrypted_len;
            break;
        case TYPE_YOUTH:
            res_transform = gzip_decompress_data(file_content, read_bytes, &processed_content, &processed_size);
            free(file_content);
            if (res_transform != 0) {
                if (processed_content) free(processed_content);
                return -EIO; 
            }
            break;
        default: 
            free(file_content);
            return -EIO;
    }

    if (offset < processed_size) {
        if (offset + size > processed_size) {
            size = processed_size - offset;
        }
        memcpy(buf, processed_content + offset, size);
    } else {
        size = 0;
    }

    if (processed_content) free(processed_content);
    return size;
}
```

Fungsi dari kode ini adalah implementasi handler untuk operasi membaca file pada filesystem FUSE yang kamu buat. Fungsi ini meng-handle pembacaan isi file dengan perlakuan khusus sesuai area "chiho"-nya.

```c
static int maimai_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    char real_path[PATH_MAX];
    char original_filename[PATH_MAX];
    ChihoType type_of_fuse_path = get_chiho_type_and_real_path(path, real_path, original_filename);

    if (real_path[0] == '\0') return -ENOENT; 

    ChihoType effective_type = type_of_fuse_path;
     if (type_of_fuse_path == TYPE_7SREF) {
        if (strstr(real_path, "/starter/")) effective_type = TYPE_STARTER;
        else if (strstr(real_path, "/metro/")) effective_type = TYPE_METRO;
        else if (strstr(real_path, "/dragon/")) effective_type = TYPE_DRAGON;
        else if (strstr(real_path, "/blackrose/")) effective_type = TYPE_BLACKROSE;
        else if (strstr(real_path, "/heaven/")) effective_type = TYPE_HEAVEN;
        else if (strstr(real_path, "/youth/")) effective_type = TYPE_YOUTH;
        else return -ENOENT;
    }

    if (effective_type == TYPE_UNKNOWN || (type_of_fuse_path == TYPE_UNKNOWN && strlen(real_path) == 0)) return -ENOENT;
    if (effective_type == TYPE_ROOT) return -EISDIR;

    int fd_write; 
    ssize_t res_write; 
    char *data_to_write = NULL;
    size_t data_to_write_len = 0;

    char *mutable_buf = (char*)malloc(size);
    if (!mutable_buf) return -ENOMEM;
    memcpy(mutable_buf, buf, size);

    switch (effective_type) {
        case TYPE_STARTER: 
        case TYPE_BLACKROSE:
            data_to_write = mutable_buf; 
            data_to_write_len = size;
            mutable_buf = NULL; 
            break;
        case TYPE_METRO:
            metro_transform(mutable_buf, size, 1); 
            data_to_write = mutable_buf;
            data_to_write_len = size;
            mutable_buf = NULL;
            break;
        case TYPE_DRAGON:
            rot13_transform(mutable_buf, size); 
            data_to_write = mutable_buf;
            data_to_write_len = size;
            mutable_buf = NULL;
            break;
        case TYPE_HEAVEN: {
            unsigned char iv[AES_BLOCK_SIZE];
            if (!RAND_bytes(iv, sizeof(iv))) { 
                free(mutable_buf);
                return -EIO; 
            }
            data_to_write = (char*)malloc(AES_BLOCK_SIZE + size + AES_BLOCK_SIZE); 
            if (!data_to_write) {
                free(mutable_buf);
                return -ENOMEM;
            }
            memcpy(data_to_write, iv, AES_BLOCK_SIZE); 

            int encrypted_len;
            if (aes_crypt((unsigned char*)mutable_buf, size,
                          (unsigned char*)data_to_write + AES_BLOCK_SIZE, &encrypted_len, 1, iv) != 0) {
                free(mutable_buf);
                free(data_to_write);
                return -EIO; 
            }
            data_to_write_len = AES_BLOCK_SIZE + encrypted_len;
            free(mutable_buf);
            mutable_buf = NULL;
            break;
        }
        case TYPE_YOUTH:
            if (gzip_compress_data(mutable_buf, size, &data_to_write, &data_to_write_len) != 0) {
                free(mutable_buf);
                if(data_to_write) free(data_to_write);
                return -EIO; 
            }
            free(mutable_buf);
            mutable_buf = NULL;
            break;
        default:
            free(mutable_buf);
            return -EIO;
    }

    if (mutable_buf) free(mutable_buf); 

    if (offset == 0 || effective_type == TYPE_STARTER || effective_type == TYPE_BLACKROSE) {
         fd_write = open(real_path, O_WRONLY | O_CREAT | (offset == 0 ? O_TRUNC : 0), 0644);
         if (fd_write == -1) {
            if (data_to_write) free(data_to_write);
            return -errno;
         }
         res_write = pwrite(fd_write, data_to_write, data_to_write_len, offset);
         if (res_write == -1) res_write = -errno;
         close(fd_write);
    } else {
        if (data_to_write) free(data_to_write);
        fprintf(stderr, "Write with offset != 0 not fully supported for transformed chihos in this simple example.\n");
        return -EIO; 
    }

    if (data_to_write) free(data_to_write);                                             
    return (res_write < 0) ? res_write : size;
}
```

Fungsi dari kode ini adalah implementasi handler menulis file (maimai_write) untuk filesystem FUSE-mu, yang menangani penulisan data ke file dengan perlakuan khusus berdasarkan tipe chiho (area) tempat file itu berada.

```c
static int maimai_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    ChihoType type = get_chiho_type_and_real_path(path, real_path, NULL);

    if (real_path[0] == '\0') return -ENOENT; 

    if (type == TYPE_UNKNOWN && strlen(real_path) == 0) return -ENOENT;
    if (type == TYPE_ROOT) return -EPERM; 

    int fd_create = open(real_path, fi->flags | O_CREAT | O_TRUNC, mode); 
    if (fd_create == -1) return -errno;
    close(fd_create);
    return 0;
}
```

Fungsi dari kode ini adalah untuk Implementasi fungsi maimai_create yang kamu berikan sudah cukup baik dan ringkas untuk membuat file baru di path FUSE-mu dengan mode dan flag yang diberikan.

```c
static int maimai_unlink(const char *path) {
    char real_path[PATH_MAX];
    ChihoType type = get_chiho_type_and_real_path(path, real_path, NULL);

    if (real_path[0] == '\0') return -ENOENT; 

    if (type == TYPE_UNKNOWN && strlen(real_path) == 0) return -ENOENT;
    if (type == TYPE_ROOT) return -EPERM;

    int res_unlink = unlink(real_path); 
    if (res_unlink == -1) return -errno;

    return 0;
}
```

Kode tersebut berfungsi untuk menghapus file.

```c
static int maimai_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    (void)fi; 
    return 0;
}
```

Fungsi dari kode ini adalah implementasi standar kosong untuk FUSE yang artinya saat file ditutup (release), tidak ada aksi khusus yang perlu dilakukan. Ini sudah benar dan cukup untuk kebanyakan kasus, terutama jika kamu tidak menggunakan handle file khusus atau resource tambahan.


```c
static int maimai_truncate(const char *path, off_t size) { 
    char real_path[PATH_MAX];
    char original_filename[PATH_MAX];
    ChihoType type_of_fuse_path = get_chiho_type_and_real_path(path, real_path, original_filename);

    if (real_path[0] == '\0') return -EIO; 

    ChihoType effective_type = type_of_fuse_path;
    if (type_of_fuse_path == TYPE_7SREF) {
        if (strstr(real_path, "/starter/")) effective_type = TYPE_STARTER;
        else if (strstr(real_path, "/metro/")) effective_type = TYPE_METRO;
        else if (strstr(real_path, "/blackrose/")) effective_type = TYPE_BLACKROSE;
        else return -EIO; 
    }


    if (effective_type == TYPE_STARTER || effective_type == TYPE_BLACKROSE) {
        int res_truncate = truncate(real_path, size); 
        if (res_truncate == -1) return -errno;
        return 0;
    } else {
        fprintf(stderr, "Truncate on transformed files not supported in this simple example.\n");
        return -EPERM; 
    }
}
```

Kode terebut berfungsi untuk menangani operasi truncate (memotong atau mengubah ukuran file) pada sistem file virtual yang kamu buat dengan FUSE, khususnya dalam konteks universe maimai yang kamu kelola.

```c
static struct fuse_operations maimai_oper = {
    .getattr    = maimai_getattr,   
    .readdir    = maimai_readdir,   
    .open       = maimai_open,
    .read       = maimai_read,
    .write      = maimai_write,
    .create     = maimai_create,
    .unlink     = maimai_unlink,
    .release    = maimai_release, 
    .truncate   = maimai_truncate,  
};
```

Kode tersebut berfungsi untuk mendefinisikan fungsi-fungsi callback yang akan dipanggil oleh FUSE (Filesystem in Userspace) ketika user atau sistem melakukan operasi tertentu pada filesystem virtual kamu.

```c
int main(int argc, char *argv[]) {
    if (realpath(CHIHO_BASE_DIR, REAL_CHIHO_BASE_PATH) == NULL) {
        perror("Failed to get real path for chiho base directory");
        fprintf(stderr, "Please ensure the '%s' directory exists in the current path or provide an absolute path.\n", CHIHO_BASE_DIR);
        return 1;
    }
    fprintf(stdout, "Using Chiho Base Path: %s\n", REAL_CHIHO_BASE_PATH);

    const char *chihos_to_create[] = {"starter", "metro", "dragon", "blackrose", "heaven", "youth"};
    size_t len_base = strlen(REAL_CHIHO_BASE_PATH);

    for (int i = 0; i < 6; i++) {
        char subdir_path[PATH_MAX];
        size_t len_chiho_name = strlen(chihos_to_create[i]);
        if (len_base + 1 + len_chiho_name + 1 > PATH_MAX) {
            fprintf(stderr, "Error: Path too long to create directory %s/%s. Skipping.\n", REAL_CHIHO_BASE_PATH, chihos_to_create[i]);
            continue;
        }
        snprintf(subdir_path, PATH_MAX, "%s/%s", REAL_CHIHO_BASE_PATH, chihos_to_create[i]);
        mkdir(subdir_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); 
    }

    umask(0); 
    return fuse_main(argc, argv, &maimai_oper, NULL);
}
```

Kode tersebut berfungsi sebagai entry point program FUSE filesystem yang kamu buat untuk universe "maimai".
