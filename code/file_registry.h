#ifndef FILE_REGISTRY_H
#define FILE_REGISTRY_H

#define MAX_REGISTERED_FILES 100
#define MAX_FILENAME_LEN 256
#define MAX_ID_LEN 64

// ע��һ���ļ�����¼�� packno��fino ����ʵ·��
void register_file(const char *packno, const char *fino, const char *path);

// ���� packno �� fino �����ļ�·��
const char* find_file_by_id(const char *packno, const char *fino);

#endif // FILE_REGISTRY_H
