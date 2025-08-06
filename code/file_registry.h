#ifndef FILE_REGISTRY_H
#define FILE_REGISTRY_H

#define MAX_REGISTERED_FILES 100
#define MAX_FILENAME_LEN 256
#define MAX_ID_LEN 64

// 注册一个文件，记录其 packno、fino 和真实路径
void register_file(const char *packno, const char *fino, const char *path);

// 根据 packno 和 fino 查找文件路径
const char* find_file_by_id(const char *packno, const char *fino);

#endif // FILE_REGISTRY_H
