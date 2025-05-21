# Modul 4

## soal_1

## soal_2
Pada soal ini, program dapat menyatukan beberapa file yang terpecah menjadi 14 bagian dengan format `.000`, `.001`, sampai `.014`. 

### `getattr`
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

## soal_3
## soal_4 
