#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>  // PATH_MAX
#include <stdbool.h>
#include <wait.h>

char source_dir[PATH_MAX];  // buffer path absolut ke folder sumber

int remove_directory_recursive(const char *path) {
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = 0; // 0 for success

    if (!d) {
        // Direktori tidak ada atau tidak dapat dibuka.
        // Jika errno adalah ENOENT, berarti direktori memang tidak ada, jadi anggap berhasil "dihapus".
        if (errno == ENOENT) return 0;
        perror("opendir for recursive delete");
        return -1;
    }

    struct dirent *p;
    while ((p = readdir(d)) != NULL) {
        // Lewati entri "." dan ".."
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
            continue;
        }

        char *buf = NULL;
        // Alokasikan buffer untuk path lengkap (path_lama/nama_file_baru)
        size_t len = path_len + strlen(p->d_name) + 2; // +2 untuk '/' dan null terminator
        buf = malloc(len);
        if (!buf) {
            perror("malloc for path buffer");
            r = -1;
            break;
        }
        snprintf(buf, len, "%s/%s", path, p->d_name);

        struct stat st_entry;
        if (stat(buf, &st_entry) == -1) {
            perror("stat on entry for recursive delete");
            free(buf);
            r = -1;
            break;
        }

        if (S_ISDIR(st_entry.st_mode)) { // Jika entri adalah direktori
            if (remove_directory_recursive(buf) == -1) { // Panggil rekursif
                r = -1;
                free(buf);
                break;
            }
        } else { // Jika entri adalah file
            if (unlink(buf) == -1) { // Hapus file
                perror("unlink file during recursive delete");
                r = -1;
                free(buf);
                break;
            }
        }
        free(buf);
    }
    closedir(d);

    if (r == 0) { // Jika tidak ada kesalahan sejauh ini, coba hapus direktori itu sendiri
        if (rmdir(path) == -1) {
            perror("rmdir");
            r = -1;
        }
    }

    return r; // 0 jika berhasil, -1 jika ada kesalahan
}

// --- Fungsi Download Anomali ---
void download()
{
    // Cek apakah folder 'anomali' sudah ada dan merupakan direktori
    struct stat st_anomali_dir;
    int anomali_dir_exists = (stat("anomali", &st_anomali_dir) == 0 && S_ISDIR(st_anomali_dir.st_mode));

    // Cek apakah file 'anomali.zip' sudah ada dan merupakan file biasa
    struct stat st_anomali_zip;
    int anomali_zip_exists = (stat("anomali.zip", &st_anomali_zip) == 0 && S_ISREG(st_anomali_zip.st_mode));

    bool proceed_with_download = true; // Asumsi default untuk melanjutkan

    if (anomali_dir_exists || anomali_zip_exists) {
        char response[10];
        printf("File atau folder 'anomali' atau 'anomali.zip' sudah ada.\n");
        printf("Apakah Anda ingin menggantinya? (y/n): ");
        if (fgets(response, sizeof(response), stdin) == NULL) {
            fprintf(stderr, "Gagal membaca input. Batalkan pengunduhan.\n");
            proceed_with_download = false;
        } else {
            // Hapus karakter newline jika ada
            response[strcspn(response, "\n")] = 0;
            if (strcmp(response, "y") != 0 && strcmp(response, "Y") != 0) {
                printf("Penggantian dibatalkan. Pengunduhan dilewati.\n");
                proceed_with_download = false;
            }
        }
    }

    if (!proceed_with_download) {
        return; // Keluar dari fungsi download jika tidak ada konfirmasi
    }

    // Jika folder 'anomali' sudah ada, hapus secara rekursif
    if (anomali_dir_exists) {
        printf("Menghapus folder 'anomali'...\n");
        if (remove_directory_recursive("anomali") == -1) {
            fprintf(stderr, "Gagal menghapus folder 'anomali'. Batalkan pengunduhan.\n");
            return;
        }
        printf("Folder 'anomali' berhasil dihapus.\n");
    }

    if (!opendir("anomali.zip"))
    {
        pid_t pid_download = fork();
        if (pid_download == 0)
        {
            char *download = "mkdir anomali && wget 'https://drive.google.com/uc?export=download&id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5' -O anomali.zip && sudo unzip anomali.zip && rm anomali.zip";
            char *argv_download[] = {"sh", "-c", download, NULL};
            execv("/bin/sh", argv_download);
        }
        else if (pid_download > 0) wait(NULL);
        else perror("fork");
    }
}

unsigned char hex_byte_converter(const char *hex) {
    unsigned char byte;
    sscanf(hex, "%2hhx", &byte);
    return byte;
}

void convert_to_image(const char *filename) {
    printf(">> Memproses file: %s\n", filename);

    FILE *input = fopen(filename, "r");
    if (!input) {
        perror("Gagal membuka file input");
        return;
    }

    fseek(input, 0, SEEK_END);
    long length = ftell(input);
    rewind(input);

    if (length <= 0) {
        printf("!! File kosong: %s\n", filename);
        fclose(input);
        return;
    }

    char *hex_data = malloc(length + 1);
    fread(hex_data, 1, length, input);
    hex_data[length] = '\0';
    fclose(input);

    // Hapus semua newline dan carriage return
    char *clean_hex = malloc(length + 1);
    int j = 0;
    for (int i = 0; i < length; i++) {
        if (hex_data[i] != '\n' && hex_data[i] != '\r') {
            clean_hex[j++] = hex_data[i];
        }
    }
    clean_hex[j] = '\0';
    free(hex_data);

    size_t hex_len = strlen(clean_hex);
    if (hex_len % 2 != 0) {
        printf("!! Hex length ganjil, tidak valid: %s\n", filename);
        free(clean_hex);
        return;
    }

    size_t bin_len = hex_len / 2;
    unsigned char *bin_data = malloc(bin_len);
    if (!bin_data) {
        free(clean_hex);
        return;
    }

    for (size_t i = 0; i < bin_len; i++) {
        bin_data[i] = hex_byte_converter(&clean_hex[i * 2]);
    }
    free(clean_hex);

    // Buat folder image di dalam source_dir
    char image_dir[PATH_MAX];
    snprintf(image_dir, sizeof(image_dir), "%s/image", source_dir);
    struct stat st = {0};
    if (stat(image_dir, &st) == -1) {
        if (mkdir(image_dir, 0755) == -1) {
            perror("Gagal membuat folder image");
            free(bin_data);
            return;
        }
    }

    // Ambil nama file tanpa path dan ekstensi
    const char *base_name = strrchr(filename, '/');
    base_name = base_name ? base_name + 1 : filename;

    char name_part[128];
    sscanf(base_name, "%[^.]", name_part);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    char output_filename[PATH_MAX];
    snprintf(output_filename, sizeof(output_filename),
        "%s/%s_image_%04d-%02d-%02d_%02d-%02d-%02d.png",
        image_dir, name_part,
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    FILE *output = fopen(output_filename, "wb");
    if (output) {
        fwrite(bin_data, 1, bin_len, output);
        fclose(output);

    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/conversion.log", source_dir);
    FILE *log = fopen(log_path, "a");
        if (log) {
            fprintf(log, "[%04d-%02d-%02d][%02d:%02d:%02d]: Successfully converted hexadecimal text %s to %s.\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                filename, output_filename);
            fclose(log);
        }

        printf("✓ Converted: %s -> %s\n", filename, output_filename);
    } else {
        printf("!! Gagal membuat file gambar: %s\n", output_filename);
    }

    free(bin_data);

}

// Cache file yang sudah di-convert
#define MAX_CONVERTED_FILES 100
static char converted_files[MAX_CONVERTED_FILES][PATH_MAX];
static int converted_count = 0;

static bool is_converted(const char *filepath) {
    for (int i = 0; i < converted_count; i++) {
        if (strcmp(converted_files[i], filepath) == 0)
            return true;
    }
    return false;
}

static void mark_converted(const char *filepath) {
    if (converted_count < MAX_CONVERTED_FILES) {
        strncpy(converted_files[converted_count], filepath, PATH_MAX);
        converted_files[converted_count][PATH_MAX - 1] = '\0'; // safety null terminator
        converted_count++;
    }
}

static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    char realpath[PATH_MAX];
    snprintf(realpath, sizeof(realpath), "%s%s", source_dir, path);

    printf("[getattr] path=%s, realpath=%s\n", path, realpath);

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    struct stat temp_stat;
    if (stat(realpath, &temp_stat) == -1)
        return -ENOENT;

    if (S_ISREG(temp_stat.st_mode)) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = temp_stat.st_size;
        return 0;
    }

    if (S_ISDIR(temp_stat.st_mode)) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    return -ENOENT;
}

static int x_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                     off_t offset, struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    DIR *dp = opendir(source_dir);
    if (!dp)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));

        if (de->d_type == DT_DIR)
            st.st_mode = S_IFDIR | 0755;
        else if (de->d_type == DT_REG)
            st.st_mode = S_IFREG | 0444;
        else
            st.st_mode = S_IFREG | 0444;

        filler(buf, de->d_name, &st, 0, 0);
    }

    closedir(dp);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s%s", source_dir, path);

    printf("[open] path=%s, realpath=%s\n", path, fullpath);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) return -ENOENT;
    close(fd);

    if (!is_converted(fullpath)) {
        convert_to_image(fullpath);
        mark_converted(fullpath);
    }

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    (void) fi;

    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s%s", source_dir, path);

    printf("[read] path=%s, realpath=%s, size=%zu, offset=%ld\n", path, fullpath, size, offset);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}

static struct fuse_operations fs_oper = {
    .getattr = fs_getattr,
    .readdir = x_readdir,
    .open = fs_open,
    .read = fs_read,
};

int main(int argc, char *argv[]) {
    download();
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }
    snprintf(source_dir, sizeof(source_dir), "%s/anomali", cwd);
    printf("Source directory set to: %s\n", source_dir);

    return fuse_main(argc, argv, &fs_oper, NULL);
}
