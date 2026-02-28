#ifndef FS_H
#define FS_H

#define FS_MAX_FILES   64
#define FS_MAX_NAME    32
#define FS_MAX_CONTENT 4096
#define FS_MAX_PATH    128

typedef struct {
    char   name[FS_MAX_NAME];
    char   content[FS_MAX_CONTENT];
    int    size;
    int    is_dir;
    int    used;
    int    parent;  /* index of parent directory, -1 for root */
} fs_node_t;

void fs_init(void);
int  fs_list(const char *path, char *out, int out_len);
int  fs_read(const char *path, char *out, int out_len);
int  fs_write(const char *path, const char *content);
int  fs_delete(const char *path);
int  fs_mkdir(const char *path);
int  fs_exists(const char *path);

#endif
