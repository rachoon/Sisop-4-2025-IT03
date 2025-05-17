#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH_LEN 1024
#define LOG_FILE "/var/log/it24.log"
#define KEYWORD1 "nafis"
#define KEYWORD2 "kimcun"

void rot13(char *str) {
    for (; *str; ++str) {
        if ((*str >= 'a' && *str <= 'm') || (*str >= 'A' && *str <= 'M')) {
            *str += 13;
        } else if ((*str >= 'n' && *str <= 'z') || (*str >= 'N' && *str <= 'Z')) {
            *str -= 13;
        }
    }
}

void reverse_string(char *str) {
    if (!str) return;
    int i = 0, j = strlen(str) - 1;
    while (i < j) {
        char c = str[i];
        str[i++] = str[j];
        str[j--] = c;
    }
}

int is_dangerous(const char *filename) {
    return strstr(filename, KEYWORD1) || strstr(filename, KEYWORD2);
}

void log_activity(const char *filename, const char *action) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] %s: %s\n", ctime(&now), action, filename);
        fclose(log);
    }
}

static int antink_getattr(const char *path, struct stat *st) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "/it24_host%s", path);
    return lstat(full_path, st) ? -errno : 0;
}

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

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "/it24_host%s", path);
    return open(full_path, fi->flags) == -1 ? -errno : 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    int fd, res;
    char full_path[MAX_PATH_LEN];
    
    snprintf(full_path, sizeof(full_path), "/it24_host%s", path);
    if ((fd = open(full_path, O_RDONLY)) == -1) return -errno;
    
    if ((res = pread(fd, buf, size, offset)) == -1) res = -errno;
    
    if (!is_dangerous(path)) {
        rot13(buf);
    }
    
    close(fd);
    return res;
}

static struct fuse_operations antink_ops = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open = antink_open,
    .read = antink_read,
};

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &antink_ops, NULL);
}
