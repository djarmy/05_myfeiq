#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#include "file_transfer_tcp.h"
#include "user_list.h"
#include "msg_handler.h"
#include "sys_info.h" 
#include "file_registry.h"

#define TCP_PORT 2425  // 飞秋默认监听端口为 2425
#define FILE_CHUNK_SIZE 4096


// 全局提示消息缓冲区
static char last_recv_file_msg[PATH_MAX * 2] = {0};
// 提示是否需要展示
static volatile int has_new_file_msg = 0;

// 提供接口供 ui_main_loop() 调用
const char* get_last_recv_file_msg() 
{
    if (has_new_file_msg) 
    {
        has_new_file_msg = 0;  // 取走就清除
        return last_recv_file_msg;
    }
    return NULL;
}



static pthread_t tcp_file_server_thread = {0};

// ============================ 接收端 =============================

static void *tcp_file_server_thread_func(void *arg)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[TCP] 创建监听 socket 失败");
        pthread_exit(NULL);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(TCP_PORT)
    };

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[TCP] bind 失败");
        close(server_fd);
        pthread_exit(NULL);
    }

    if (listen(server_fd, 5) < 0) {
        perror("[TCP] listen 失败");
        close(server_fd);
        pthread_exit(NULL);
    }

    //printf("[TCP] 文件接收服务启动，监听端口: %d\n", TCP_PORT);

    while (1) 
    {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("[TCP] accept 失败");
            continue;
        }

        char file_name[PATH_MAX] = {0};
        char buffer[FILE_CHUNK_SIZE] = {0};

        // // 接收文件名
        // ssize_t name_len = recv(client_fd, file_name, sizeof(file_name) - 1, 0);
        // if (name_len <= 0) {
        //     perror("[TCP] 文件名接收失败");
        //     close(client_fd);
        //     continue;
        // }
           // ✅ 第一次 recv 直接收 GETFILEDATA 命令
        char request[256] = {0};
        ssize_t name_len = recv(client_fd, request, sizeof(request) - 1, 0);
        if (name_len <= 0) {
            perror("[TCP] 接收 GETFILEDATA 失败");
            close(client_fd);
            continue;
        }
        request[name_len] = '\0';
        printf("[TCP] 收到请求：%s\n", request);

        unsigned long packet_no = 0;
        unsigned int file_id = 0;
        if (sscanf(request, "GETFILEDATA %lu %u", &packet_no, &file_id) != 2) {
            fprintf(stderr, "[TCP] 无效请求：%s\n", request);
            close(client_fd);
            continue;
        }
        file_name[name_len] = '\0';  // 显式添加 null 终结符，确保字符串安全

        // 拼接保存路径（推荐加 recv_ 前缀）
        char saved_name[PATH_MAX] = {0};
        snprintf(saved_name, sizeof(saved_name), "recv_%s", file_name);
        int fd = open(saved_name, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            perror("[TCP] 打开接收文件失败");
            close(client_fd);
            continue;
        }

         memset(request, 0, sizeof(request));  // 正确：用memset清空数组
         
        printf("[TCP] 收到请求：%s\n", request);
 

        // 需要将 packet_no 和 file_id 转换为字符串再传递
        char packet_no_str[32];
        char file_id_str[32];
        snprintf(packet_no_str, sizeof(packet_no_str), "%lu", packet_no);
        snprintf(file_id_str, sizeof(file_id_str), "%u", file_id);

        const char* filepath = find_file_by_id(packet_no_str, file_id_str);
        if (!filepath) {
            fprintf(stderr, "[TCP] file_id=%d 未找到对应路径\n", file_id);
            close(client_fd);
            continue;
        }
        fd = open(filepath, O_RDONLY);
        if (fd < 0) {
            perror("[TCP] 打开文件失败");
            close(client_fd);
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes;
        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
            if (send(client_fd, buffer, bytes, 0) < 0) {
                perror("[TCP] 发送失败");
                break;
            }
        }
        printf("[TCP] 文件发送完成：%s\n", filepath);

        close(fd);
        close(client_fd);
    }

    close(server_fd);
    pthread_exit(NULL);
}

// 对外暴露的接口：启动 TCP 接收线程
void init_tcp_file_transfer_server(void)
{
    //printf("[调试] 正在启动 TCP 接收线程...\n");   // 加一行调试
    int ret = pthread_create(&tcp_file_server_thread, NULL, tcp_file_server_thread_func, NULL);
    if (ret != 0) 
    {
        perror("[错误] 创建 TCP 接收线程失败");
        return;
    }
    pthread_detach(tcp_file_server_thread);
}

// ============================ 发送端 =============================

void* tcp_file_send_thread_func(void *arg) 
{
    tcp_file_send_args_t *args = (tcp_file_send_args_t *)arg;

    const char *ipaddr = args->ip;
    const char *filepath = args->file_path;
    uint16_t port = args->port;
    free(arg);  // 参数复制完成后立即释放堆内存

    // 打开文件
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("[TCP] 打开文件失败");
        pthread_exit(NULL);
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("[TCP] 获取文件信息失败");
        close(fd);
        pthread_exit(NULL);
    }

    // 创建 socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[TCP] 创建 socket 失败");
        close(fd);
        pthread_exit(NULL);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (inet_pton(AF_INET, ipaddr, &server_addr.sin_addr) <= 0) {
        perror("[TCP] IP 地址无效");
        close(fd);
        close(sock);
        pthread_exit(NULL);
    }

    // 设置 connect 超时（可选）
    struct timeval timeout = {3, 0}; // 3 秒超时
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[TCP] 连接服务器失败");
        close(fd);
        close(sock);
        pthread_exit(NULL);
    }

    // 发送文件名
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    send(sock, filename, strlen(filename), 0);
    send(sock, "\n", 1, 0);  // 简单分隔

    // 发送文件数据
    char buffer[FILE_CHUNK_SIZE];
    ssize_t read_bytes;
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        if (send(sock, buffer, read_bytes, 0) < 0) {
            perror("[TCP] 发送数据失败");
            break;
        }
    }

    printf("[TCP] 文件发送完成: %s\n", filepath);
    close(fd);
    close(sock);
    pthread_exit(NULL);
}

// 生成唯一 file_id（可用 hash，也可以用递增）
int generate_file_id(void) 
{
    static int current_id = 100;  // 起始编号
    return current_id++;
}


//重写 start_tcp_file_send() 启动线程
bool start_tcp_file_send(const char* ipaddr, uint16_t port, const char* filepath) 
{
    if (!ipaddr || !filepath) {
        fprintf(stderr, "[TCP] 参数异常\n");
        return false;
    }


    // 1. 生成 packet_no（数据包编号，作为 packno 参数）
    unsigned long packet_no = (unsigned long)time(NULL);  // 用当前时间作为简单的包编号
    char packno_str[32];
    snprintf(packno_str, sizeof(packno_str), "%lu", packet_no);  // 转换为字符串

    // 2. 生成 file_id 并转换为字符串（作为 fino 参数）
    int file_id = generate_file_id();
    char fino_str[32];
    snprintf(fino_str, sizeof(fino_str), "%d", file_id);  // 整数转字符串

    // 3. 完整调用 register_file，传入3个字符串参数
    register_file(packno_str, fino_str, filepath);  // 注意：原函数返回 void，不能用 if 判断

    tcp_file_send_args_t *args = malloc(sizeof(tcp_file_send_args_t));
    if (!args) {
        perror("[TCP] 内存分配失败");
        return false;
    }

    strncpy(args->ip, ipaddr, sizeof(args->ip) - 1);
    args->port = port;
    strncpy(args->file_path, filepath, sizeof(args->file_path) - 1);

    pthread_t tid;
    if (pthread_create(&tid, NULL, tcp_file_send_thread_func, args) != 0) {
        perror("[TCP] 创建线程失败");
        free(args);
        return false;
    }

    pthread_detach(tid);  // 分离线程，自动回收资源
    return true;
}