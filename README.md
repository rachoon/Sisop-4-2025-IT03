# Modul 4

## soal_1

## soal_2
Pada soal ini, program dapat menyatukan beberapa file yang terpecah menjadi 14 bagian dengan format `.000`, `.001`, sampai `.014`. 

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
10. `free(files[i]` digunakan untuk membersihkan memori untuk emnghindari terjadinya memory leak.
## soal_3
## soal_4 
