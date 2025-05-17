#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>

#define ROOT_DIR "./chiho"

// list area valid
const char *areas[] = {
    "starter", "metro", "dragon", "blackrose", "heaven", "youth", "7sref", NULL
};

// cek nama folder valid atau tidak
static int is_area(const char *name) {
    for (int i = 0; areas[i]; i++) {
        if (strcmp(name, areas[i]) == 0) return 1;
    }
    return 0;
}

// memisahkan path menjadi area dan subpath
static void parse_path(const char *path, char *area, char *subpath) {
    area[0] = 0; subpath[0] = 0;
    if (path[0] == '/') path++;
    const char *slash = strchr(path, '/');
    if (!slash) {
        strcpy(area, path);
        return;
    }
    strncpy(area, path, slash - path);
    area[slash - path] = 0;
    strcpy(subpath, slash + 1);
}

// encrypt dan decrypt untuk area dragon
static void rot13(char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if ('a' <= c && c <= 'z') buf[i] = 'a' + (c - 'a' + 13) % 26;
        else if ('A' <= c && c <= 'Z') buf[i] = 'A' + (c - 'A' + 13) % 26;
    }
}

// menambahkan .mai jika subpath belum berakhiran .mai
static void starter_to_real_path(const char *subpath, char *realpath) {
    if (strlen(subpath) > 4 && strcmp(subpath + strlen(subpath) - 4, ".mai") == 0) {
        sprintf(realpath, "%s/starter/%s", ROOT_DIR, subpath);
    } else {
        sprintf(realpath, "%s/starter/%s.mai", ROOT_DIR, subpath);
    }
}

// nama file diubah dengan shift +1
static void metro_to_real_path(const char *subpath, char *realpath) {
    char shifted[256];
    int len = strlen(subpath);
    for (int i = 0; i < len; i++) {
        char c = subpath[i];
        if (isalpha(c)) {
            if (c == 'z') c = 'a';
            else if (c == 'Z') c = 'A';
            else c += 1;
        }
        shifted[i] = c;
    }
    shifted[len] = 0;
    sprintf(realpath, "%s/metro/%s", ROOT_DIR, shifted);
}

// area_namafile menjadi pointer ke file asli di area lain (area 7sref)
static int ref_to_real_path(const char *subpath, char *realpath) {
    // format: area_filename
    const char *under = strchr(subpath, '_');
    if (!under) return -1;
    char area[32];
    size_t len = under - subpath;
    if (len >= sizeof(area)) return -1;
    strncpy(area, subpath, len);
    area[len] = 0;
    if (!is_area(area)) return -1;
    const char *filename = under + 1;
    sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, filename);
    return 0;
}

// ===== FUSE operations =====

// mengambil informasi file atau direktori
static int maimai_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) == 0) {
        // direktori area
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    char realpath[512];
    if (strcmp(area, "starter") == 0) {
        starter_to_real_path(subpath, realpath);
    } else if (strcmp(area, "metro") == 0) {
        metro_to_real_path(subpath, realpath);
    } else if (strcmp(area, "7sref") == 0) {
        if (ref_to_real_path(subpath, realpath) < 0) return -ENOENT;
    } else {
        sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, subpath);
    }
    int res = lstat(realpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

// membaca isi direktori
static int maimai_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
            off_t offset, struct fuse_file_info *fi,
            enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    if (strcmp(path, "/") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        for (int i = 0; areas[i]; i++) {
            filler(buf, areas[i], NULL, 0, 0);
        }
        return 0;
    }

    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) != 0) return -ENOENT;

    char dirpath[512];
    sprintf(dirpath, "%s/%s", ROOT_DIR, area);
    DIR *dp = opendir(dirpath);
    if (dp == NULL) return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        // Area-specific filter
        if (strcmp(area, "starter") == 0) {
            // tampilkan nama tanpa .mai
            size_t len = strlen(de->d_name);
            if (len > 4 && strcmp(de->d_name + len - 4, ".mai") == 0) {
                char tmp[256];
                strncpy(tmp, de->d_name, len - 4);
                tmp[len - 4] = 0;
                filler(buf, tmp, NULL, 0, 0);
            }
        } else if (strcmp(area, "metro") == 0) {
            // tampilkan nama dengan shift -1
            char tmp[256];
            int len = strlen(de->d_name);
            for (int i = 0; i < len; i++) {
                char c = de->d_name[i];
                if (isalpha(c)) {
                    if (c == 'a') c = 'z';
                    else if (c == 'A') c = 'Z';
                    else c -= 1;
                }
                tmp[i] = c;
            }
            tmp[len] = 0;
            filler(buf, tmp, NULL, 0, 0);
        } else if (strcmp(area, "7sref") == 0) {
            // tampilkan dengan format area_filename
            // untuk setiap file di semua area folder, kecuali 7sref sendiri
            for (int i = 0; areas[i]; i++) {
                if (strcmp(areas[i], "7sref") == 0) continue;
                char apath[512];
                sprintf(apath, "%s/%s", ROOT_DIR, areas[i]);
                DIR *adp = opendir(apath);
                if (!adp) continue;
                struct dirent *ade;
                while ((ade = readdir(adp)) != NULL) {
                    if (ade->d_name[0] == '.') continue;
                    char combined[512];
                    snprintf(combined, sizeof(combined), "%s_%s", areas[i], ade->d_name);
                    filler(buf, combined, NULL, 0, 0);
                }
                closedir(adp);
            }
            closedir(dp);
            return 0;
        } else {
            // lainnya tampilkan nama file apa adanya
            if (de->d_name[0] != '.') {
                filler(buf, de->d_name, NULL, 0, 0);
            }
        }
    }
    closedir(dp);
    return 0;
}

// membuka akses file baca dan tulis dan tentukan path sesuai area
static int maimai_open(const char *path, struct fuse_file_info *fi) {
    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) == 0) return -EISDIR;

    char realpath[512];
    if (strcmp(area, "starter") == 0) {
        starter_to_real_path(subpath, realpath);
    } else if (strcmp(area, "metro") == 0) {
        metro_to_real_path(subpath, realpath);
    } else if (strcmp(area, "7sref") == 0) {
        if (ref_to_real_path(subpath, realpath) < 0) return -ENOENT;
    } else {
        sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, subpath);
    }

    int fd = open(realpath, fi->flags);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}

// membaca isi file dari offset 
static int maimai_read(const char *path, char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
    (void) fi;
    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) == 0) return -EISDIR;

    char realpath[512];
    if (strcmp(area, "starter") == 0) {
        starter_to_real_path(subpath, realpath);
    } else if (strcmp(area, "metro") == 0) {
        metro_to_real_path(subpath, realpath);
    } else if (strcmp(area, "7sref") == 0) {
        if (ref_to_real_path(subpath, realpath) < 0) return -ENOENT;
    } else {
        sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, subpath);
    }

    FILE *f = fopen(realpath, "rb");
    if (!f) return -errno;
    fseek(f, 0, SEEK_END);
    size_t filesize = ftell(f);
    if (offset > filesize) {
        fclose(f);
        return 0;
    }
    size_t toread = size;
    if (offset + size > filesize) toread = filesize - offset;
    fseek(f, offset, SEEK_SET);
    size_t nread = fread(buf, 1, toread, f);
    fclose(f);

    if (strcmp(area, "dragon") == 0) {
        // decrypt ROT13
        rot13(buf, nread);
    }

    return nread;
}

// menulis data ke file di offset 
static int maimai_write(const char *path, const char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
    (void) fi;
    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) == 0) return -EISDIR;

    char realpath[512];
    if (strcmp(area, "starter") == 0) {
        starter_to_real_path(subpath, realpath);
    } else if (strcmp(area, "metro") == 0) {
        metro_to_real_path(subpath, realpath);
    } else if (strcmp(area, "7sref") == 0) {
        if (ref_to_real_path(subpath, realpath) < 0) return -ENOENT;
    } else {
        sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, subpath);
    }

    FILE *f = fopen(realpath, "r+b");
    if (!f) {
        // buat baru
        f = fopen(realpath, "w+b");
        if (!f) return -errno;
    }

    // isi kosong sampai offset kalo butuh
    fseek(f, 0, SEEK_END);
    size_t filesize = ftell(f);
    if (offset > filesize) {
        fseek(f, filesize, SEEK_SET);
        for (size_t i = filesize; i < offset; i++) fputc(0, f);
    }
    fseek(f, offset, SEEK_SET);

    if (strcmp(area, "dragon") == 0) {
        // encrypt ROT13
        char *encbuf = malloc(size);
        if (!encbuf) {
            fclose(f);
            return -ENOMEM;
        }
        memcpy(encbuf, buf, size);
        rot13(encbuf, size);
        fwrite(encbuf, 1, size, f);
        free(encbuf);
    } else {
        fwrite(buf, 1, size, f);
    }

    fclose(f);
    return size;
}

// membuat file baru
static int maimai_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) == 0) return -EISDIR;

    char realpath[512];
    if (strcmp(area, "starter") == 0) {
        starter_to_real_path(subpath, realpath);
    } else if (strcmp(area, "metro") == 0) {
        metro_to_real_path(subpath, realpath);
    } else if (strcmp(area, "7sref") == 0) {
        if (ref_to_real_path(subpath, realpath) < 0) return -ENOENT;
    } else {
        sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, subpath);
    }

    int fd = open(realpath, fi->flags, mode);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}

// menghapus file
static int maimai_unlink(const char *path) {
    char area[32], subpath[256];
    parse_path(path, area, subpath);
    if (!is_area(area)) return -ENOENT;
    if (strlen(subpath) == 0) return -EISDIR;

    char realpath[512];
    if (strcmp(area, "starter") == 0) {
        starter_to_real_path(subpath, realpath);
    } else if (strcmp(area, "metro") == 0) {
        metro_to_real_path(subpath, realpath);
    } else if (strcmp(area, "7sref") == 0) {
        if (ref_to_real_path(subpath, realpath) < 0) return -ENOENT;
    } else {
        sprintf(realpath, "%s/%s/%s", ROOT_DIR, area, subpath);
    }
    int res = unlink(realpath);
    if (res == -1) return -errno;
    return 0;
}

// implementasi fungsi ke fuse
static struct fuse_operations maimai_oper = {
    .getattr    = maimai_getattr,
    .readdir    = maimai_readdir,
    .open       = maimai_open,
    .read       = maimai_read,
    .write      = maimai_write,
    .create     = maimai_create,
    .unlink     = maimai_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &maimai_oper, NULL);
}
