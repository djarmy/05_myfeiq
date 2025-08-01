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

#define TCP_PORT 9527
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

    printf("[TCP] 文件接收服务启动，监听端口: %d\n", TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("[TCP] accept 失败");
            continue;
        }

        char file_name[PATH_MAX] = {0};
        char buffer[FILE_CHUNK_SIZE] = {0};

        // 接收文件名
        ssize_t name_len = recv(client_fd, file_name, sizeof(file_name) - 1, 0);
        if (name_len <= 0) {
            perror("[TCP] 文件名接收失败");
            close(client_fd);
            continue;
        }
        file_name[name_len] = '\0';  // 显式添加 null 终结符，确保字符串安全

        // 拼接保存路径（推荐加 recv_ 前缀）
        char saved_name[PATH_MAX] = {0};
        snprintf(saved_name, sizeof(file_name), "recv_%s", file_name);
        int fd = open(saved_name, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            perror("[TCP] 打开接收文件失败");
            close(client_fd);
            continue;
        }

        ssize_t received;
        while ((received = recv(client_fd, buffer, FILE_CHUNK_SIZE, 0)) > 0) {
            write(fd, buffer, received);
        }

        // ✅ 打印到终端提示
        char ipstr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, sizeof(ipstr));

        // ✅ 设置提示供 UI 显示
        snprintf(last_recv_file_msg, sizeof(last_recv_file_msg),
            "[TCP] 文件接收成功！\n"
            "      发送方：%s\n"
            "      文件名：%s\n"
            "      已保存至：%s/%s\n",
            ipstr, saved_name, getcwd(NULL, 0), saved_name);
        has_new_file_msg = 1;

        safe_log("[TCP] 接收完成: %s\n", saved_name);
        close(fd);
        close(client_fd);
    }

    close(server_fd);
    pthread_exit(NULL);
}

void init_tcp_file_transfer_server(void)
{
    pthread_create(&tcp_file_server_thread, NULL, tcp_file_server_thread_func, NULL);
    pthread_detach(tcp_file_server_thread);
}

// ============================ 发送端 =============================

bool start_tcp_file_send(const char* ipaddr, uint16_t port, const char* filepath)
{
    if (!ipaddr || !filepath || access(filepath, R_OK) != 0) {
        fprintf(stderr, "[TCP] 参数无效或文件不可读\n");
        return false;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("[TCP] 打开文件失败");
        return false;
    }

    struct stat st = {0};
    if (fstat(fd, &st) < 0) {
        perror("[TCP] 获取文件大小失败");
        close(fd);
        return false;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[TCP] 创建 socket 失败");
        close(fd);
        return false;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (inet_pton(AF_INET, ipaddr, &server_addr.sin_addr) <= 0) {
        perror("[TCP] IP 格式无效");
        close(fd);
        close(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[TCP] 连接服务器失败");
        close(fd);
        close(sock);
        return false;
    }

    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    send(sock, filename, strlen(filename), 0);  // 发送文件名

    char buffer[FILE_CHUNK_SIZE] = {0};
    ssize_t read_bytes;
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        send(sock, buffer, read_bytes, 0);
    }

    printf("[TCP] 文件发送完成: %s\n", filepath);
    close(fd);
    close(sock);
    return true;
}
