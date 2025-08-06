#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <netinet/in.h>  // 用于 struct sockaddr_in
#include <stdbool.h>     // 用于 bool 类型
#include <stddef.h>      // 用于 size_t 
#include <limits.h>  // 包含 PATH_MAX 定义

#define FILE_UDP_PORT 2425

//每次发送文件块的大小
#define FILE_CHUNK_SIZE 4096

//文件数据：文件数据传输的参数结构体
typedef struct 
{
    struct sockaddr_in dest_addr;    //指定目标地址
    char file_path[PATH_MAX];                //文件路径（假设不超过 255 字节）
}file_send_args_t;


//文件传输初始化函数(例如打开socket)
void init_file_transfer(void);

//启动发送文件线程(目标绝对路径，目标接受地址)
bool start_file_send(const struct sockaddr_in *dest_addr, const char *file_path);

//安全清理释放文件传输资源（如关闭socket）
void cleanup_file_transfer(void);

// 从用户编号和文件路径，发送文件
void send_file_to_user(int user_id, const char *filepath);

#define FILE_RECV_PORT 2425
#define FILE_BUFFER_SIZE 4096

//文件接收端函数
static void* udp_file_receiver_thread(void *arg);

//启动udp接受线程
void start_udp_file_receiver(void);

static void send_ipmsg_file_announce(const struct sockaddr_in *dest_addr, const char *username, const char *hostname, const char *filepath, const char *file_id);

#endif // !FILE_TRANSFER_H