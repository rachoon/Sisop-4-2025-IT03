# Modul 4

## soal_1

## soal_2
Pada soal ini, program dapat menyatukan beberapa file yang terpecah menjadi 14 bagian dengan format `.000`, `.001`, sampai `.013`. 

### A. `baymax_getattr`
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

### B. `baymax_readdir`
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
Dimana untuk log_activity akan dijabarkan pada ... .

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
## soal_3
## soal_4 
