

#ifndef FILE_TRANSFER_TCP_H
#define FILE_TRANSFER_TCP_H

#include <stdbool.h>
#include <stdint.h>

// 初始化 TCP 文件接收服务（监听端口并创建接收线程）
void init_tcp_file_transfer_server(void);

// 启动一个 TCP 文件发送线程
bool start_tcp_file_send(const char* ipaddr, uint16_t port, const char* filepath);

// 清理 TCP 文件传输资源
void cleanup_tcp_file_transfer(void);

// 提供接口供 ui_main_loop() 调用
const char* get_last_recv_file_msg();

#endif
