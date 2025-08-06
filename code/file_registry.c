#include <string.h>
#include <stdio.h>
#include "file_registry.h"

typedef struct {
    char packno[MAX_ID_LEN];
    char fino[MAX_ID_LEN];
    char path[MAX_FILENAME_LEN];
    int valid;
} FileEntry;

static FileEntry file_table[MAX_REGISTERED_FILES];

// 注册文件
void register_file(const char *packno, const char *fino, const char *path) {
    for (int i = 0; i < MAX_REGISTERED_FILES; ++i) {
        if (!file_table[i].valid) {
            strncpy(file_table[i].packno, packno, MAX_ID_LEN - 1);
            strncpy(file_table[i].fino, fino, MAX_ID_LEN - 1);
            strncpy(file_table[i].path, path, MAX_FILENAME_LEN - 1);
            file_table[i].valid = 1;
            return;
        }
    }
    fprintf(stderr, "[registry] 文件注册表已满，无法注册 %s\n", path);
}

// 查找文件路径
const char* find_file_by_id(const char *packno, const char *fino) {
    for (int i = 0; i < MAX_REGISTERED_FILES; ++i) {
        if (file_table[i].valid &&
            strcmp(file_table[i].packno, packno) == 0 &&
            strcmp(file_table[i].fino, fino) == 0) {
            return file_table[i].path;
        }
    }
    return NULL;
}
