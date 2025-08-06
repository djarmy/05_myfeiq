

#ifndef FILE_TRANSFER_TCP_H
#define FILE_TRANSFER_TCP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char ip[64];
    uint16_t port;
    char file_path[512];
} tcp_file_send_args_t;

void* tcp_file_send_thread_func(void *arg);
bool start_tcp_file_send(const char* ipaddr, uint16_t port, const char* filepath);

// 初始化 TCP 文件接收服务（监听端口并创建接收线程）
void init_tcp_file_transfer_server(void);

// 启动一个 TCP 文件发送线程
bool start_tcp_file_send(const char* ipaddr, uint16_t port, const char* filepath);

// 清理 TCP 文件传输资源
void cleanup_tcp_file_transfer(void);

// 提供接口供 ui_main_loop() 调用
const char* get_last_recv_file_msg();
 
// 生成唯一 file_id（可用 hash，也可以用递增）
int generate_file_id(void);

#endif
