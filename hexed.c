#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static const char *source_dir = "anomali";


static int x_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if(strcmp(path, hello_path) == 0)
    {
        printf("helloooo berhasil didapat.\n");
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", source_dir, path + 1);
        FILE *f = fopen(fullpath, "rb");
        if (f)
        {
            fseek(f, 0, SEEK_END);
            stbuf->st_size = ftell(f);
            fclose(f);
        }
        else
        {
            return -ENOENT;
        }
        return 0;
    }

    return -ENOENT;
}

static int x_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0) return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, hello_path + 1, NULL, 0, 0);

    return 0;
}

static int x_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, hello_path) != 0) return -ENOENT;

    return 0;
}

static int x_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) fi;

    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", source_dir, path + 1);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) return -errno;

    struct stat st;
    if(fstat(fd, &st) == -1)
    {
        close(fd);
        return -errno;
    }

    if (offset < st.st_size)
    {
        if (offset + size > st.st_size) size = st.st_size - offset;

        if (pread(fd, buf, size, offset) == -1)
        {
            close(fd);
            return -errno;
        }
        
    }
    else size = 0;

    close(fd);
    return size;
}

static struct fuse_operations my_oper = 
{
    .getattr = x_getattr,
    .readdir = x_readdir,
    .open = x_open,
    .read = x_read,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &my_oper, NULL);
}