#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/time.h>

#define MAX_FRAGMENTS 100
#define FRAGMENT_SIZE 1024
#define LOG_FILE "activity.log"
#define OPTION(t, p) { t, offsetof(baymax_options, p), 1 }

typedef struct {
    char *relics_dir;
    char *copy_dest_dir;
    char *dest_filename;
} baymax_options;

baymax_options options = {0};

char mountpoint_path[PATH_MAX] = {0};

// Variabel global untuk menandai apakah file telah dibaca sepenuhnya
int file_dibaca_penuh = 0;

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

static int baymax_open(const char *path, struct fuse_file_info *fi) {
    char *filename = basename(strdup(path + 1));
    log_activity("READ", filename);
    free(filename);
    return 0;
}

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

static struct fuse_opt option_specs[] = {
    OPTION("--relics-dir=%s", relics_dir),
    OPTION("--copy-dest=%s", copy_dest_dir),
    OPTION("--dest-filename=%s", dest_filename),
    FUSE_OPT_END
};

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