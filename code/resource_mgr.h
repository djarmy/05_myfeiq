#ifndef RESOURCE_MGR_H
#define RESOURCE_MGR_H

// 统一释放所有资源，包括 socket、动态内存、文件句柄等
void cleanup_all_resources(void);

#endif
