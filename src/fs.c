/* ============================================================
 * SwanOS — In-Memory Filesystem
 * Simple flat filesystem with directory support.
 * All data lives in RAM (lost on reboot).
 * ============================================================ */

#include "fs.h"
#include "string.h"

static fs_node_t nodes[FS_MAX_FILES];

void fs_init(void) {
    memset(nodes, 0, sizeof(nodes));
    /* Root directory */
    strcpy(nodes[0].name, "/");
    nodes[0].is_dir = 1;
    nodes[0].used = 1;
    nodes[0].parent = -1;
}

/* Find a node by path. Returns index or -1. */
static int find_node(const char *path) {
    if (strcmp(path, "/") == 0 || strcmp(path, ".") == 0 || path[0] == '\0')
        return 0; /* root */

    /* Skip leading / */
    const char *p = path;
    if (*p == '/') p++;

    int parent = 0;
    char part[FS_MAX_NAME];

    while (*p) {
        /* Extract next path component */
        int i = 0;
        while (*p && *p != '/' && i < FS_MAX_NAME - 1) part[i++] = *p++;
        part[i] = '\0';
        if (*p == '/') p++;
        if (part[0] == '\0') continue;

        /* Search children of parent */
        int found = -1;
        for (int j = 0; j < FS_MAX_FILES; j++) {
            if (nodes[j].used && nodes[j].parent == parent && strcmp(nodes[j].name, part) == 0) {
                found = j;
                break;
            }
        }
        if (found < 0) return -1;
        parent = found;
    }
    return parent;
}

/* Find a free slot */
static int find_free(void) {
    for (int i = 1; i < FS_MAX_FILES; i++) {
        if (!nodes[i].used) return i;
    }
    return -1;
}

/* Get parent dir and basename from a path */
static int parse_path(const char *path, int *parent_idx, char *basename) {
    if (!path || path[0] == '\0') return -1;

    /* Find last / */
    const char *last_slash = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }

    if (!last_slash || last_slash == path) {
        /* File in root */
        *parent_idx = 0;
        const char *p = path;
        if (*p == '/') p++;
        strncpy(basename, p, FS_MAX_NAME - 1);
        basename[FS_MAX_NAME - 1] = '\0';
    } else {
        /* Extract parent path */
        char parent_path[FS_MAX_PATH];
        int len = last_slash - path;
        strncpy(parent_path, path, len);
        parent_path[len] = '\0';

        *parent_idx = find_node(parent_path);
        if (*parent_idx < 0) return -1;

        strncpy(basename, last_slash + 1, FS_MAX_NAME - 1);
        basename[FS_MAX_NAME - 1] = '\0';
    }

    /* Remove trailing slashes from basename */
    int blen = strlen(basename);
    while (blen > 0 && basename[blen - 1] == '/') basename[--blen] = '\0';

    return 0;
}

int fs_list(const char *path, char *out, int out_len) {
    int dir = find_node(path);
    if (dir < 0 || !nodes[dir].is_dir) {
        strcpy(out, "Not a directory.");
        return -1;
    }

    out[0] = '\0';
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (nodes[i].used && nodes[i].parent == dir) {
            if (strlen(out) + strlen(nodes[i].name) + 10 >= (unsigned int)out_len) break;
            if (nodes[i].is_dir) {
                strcat(out, "  [DIR]  ");
            } else {
                strcat(out, "  [FILE] ");
            }
            strcat(out, nodes[i].name);
            if (nodes[i].is_dir) strcat(out, "/");
            strcat(out, "\n");
            count++;
        }
    }
    if (count == 0) strcpy(out, "  (empty)\n");
    return count;
}

int fs_read(const char *path, char *out, int out_len) {
    int idx = find_node(path);
    if (idx < 0) { strcpy(out, "File not found."); return -1; }
    if (nodes[idx].is_dir) { strcpy(out, "Cannot read a directory."); return -1; }
    strncpy(out, nodes[idx].content, out_len - 1);
    out[out_len - 1] = '\0';
    return nodes[idx].size;
}

int fs_write(const char *path, const char *content) {
    int idx = find_node(path);

    if (idx >= 0) {
        /* Overwrite existing file */
        if (nodes[idx].is_dir) return -1;
        strncpy(nodes[idx].content, content, FS_MAX_CONTENT - 1);
        nodes[idx].content[FS_MAX_CONTENT - 1] = '\0';
        nodes[idx].size = strlen(nodes[idx].content);
        return 0;
    }

    /* Create new file */
    char basename[FS_MAX_NAME];
    int parent;
    if (parse_path(path, &parent, basename) < 0) return -1;
    if (parent < 0 || !nodes[parent].is_dir) return -1;

    int slot = find_free();
    if (slot < 0) return -1;

    nodes[slot].used = 1;
    nodes[slot].is_dir = 0;
    nodes[slot].parent = parent;
    strcpy(nodes[slot].name, basename);
    strncpy(nodes[slot].content, content, FS_MAX_CONTENT - 1);
    nodes[slot].content[FS_MAX_CONTENT - 1] = '\0';
    nodes[slot].size = strlen(nodes[slot].content);
    return 0;
}

int fs_delete(const char *path) {
    int idx = find_node(path);
    if (idx <= 0) return -1; /* Can't delete root or not found */

    if (nodes[idx].is_dir) {
        /* Check if empty */
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (nodes[i].used && nodes[i].parent == idx) return -2; /* Not empty */
        }
    }

    memset(&nodes[idx], 0, sizeof(fs_node_t));
    return 0;
}

int fs_mkdir(const char *path) {
    if (find_node(path) >= 0) return -1; /* Already exists */

    char basename[FS_MAX_NAME];
    int parent;
    if (parse_path(path, &parent, basename) < 0) return -1;
    if (parent < 0 || !nodes[parent].is_dir) return -1;

    int slot = find_free();
    if (slot < 0) return -1;

    nodes[slot].used = 1;
    nodes[slot].is_dir = 1;
    nodes[slot].parent = parent;
    strcpy(nodes[slot].name, basename);
    return 0;
}

int fs_exists(const char *path) {
    return find_node(path) >= 0;
}
