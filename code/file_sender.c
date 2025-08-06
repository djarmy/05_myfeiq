/* =====================
 * �ļ���file_sender.c
 * Ŀ�ģ�ʵ�ַ�����ݵ� sendfile()
 * �漰ģ�飺UDP ֪ͨ + �ļ�ע�� + TCP ʵ�ʷ���
 * ===================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "file_registry.h"
#include "sys_info.h" 
#include "IPMSG.H"

extern int get_udp_fd();
extern int get_tcp_fd();

// ===========================
// 1. UDP ֪ͨ + �ļ�ע��
// ===========================
void sendfile_ipmsg(const char* ip, const char* filepath)
{
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2425);
    addr.sin_addr.s_addr = inet_addr(ip);

    struct stat st;
    if (stat(filepath, &st) < 0)
    {
        perror("stat");
        return;
    }

    // �����ļ���
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    // ������� packno & fino
    time_t now = time(NULL);
    char packno[20];
    char fino[10];
    sprintf(packno, "%lu", now);
    strcpy(fino, "0"); // �̶�Ϊ 0������ͻ���Ĭ��֧��

    // ע�ᵽ�ļ�ע���
    register_file(packno, fino, filepath);

    // ���� UDP ��Ϣ�壨˫�Σ�
    char buf[1024] = {0};
    sprintf(buf, "1:%lu:%s:%s:%lu:%c",
            now, get_user(), get_host(),
            IPMSG_SENDMSG | IPMSG_SENDCHECKOPT | IPMSG_FILEATTACHOPT, 0);

    sprintf(buf + strlen(buf) + 1, "0:%s:%ld:%ld",
            filename, st.st_size, st.st_ctime);

    int udp_fd = get_udp_fd();
    sendto(udp_fd, buf, strlen(buf) + strlen(buf + strlen(buf) + 1),
           0, (struct sockaddr *)&addr, sizeof(addr));

    printf("[sendfile] ֪ͨ������ɣ��ļ���=%s, packno=%s, fino=%s\n", filename, packno, fino);
}

// ===========================
// 2. TCP �ļ������߳�
// ===========================
void *tcp_file_sender_thread(void *arg)
{
    int tcp_fd = get_tcp_fd();
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    while (1)
    {
        int cfd = accept(tcp_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cfd < 0)
        {
            perror("accept");
            continue;
        }

        char buf[1024] = {0};
        int len = recv(cfd, buf, sizeof(buf), 0);
        buf[len] = '\0';

        // ���� GETFILEDATA
        char *parts[10] = {0};
        int i = 0;
        parts[i++] = strtok(buf, ":");
        while ((parts[i++] = strtok(NULL, ":")) != NULL);

        if (atoi(parts[4]) == IPMSG_GETFILEDATA)
        {
            const char *packno = parts[5];
            const char *fino = parts[6];

            const char *filepath = find_file_by_id(packno, fino);
            if (!filepath)
            {
                printf("[send] δ�ҵ��ļ���packno=%s, fino=%s\n", packno, fino);
                close(cfd);
                continue;
            }

            int fd = open(filepath, O_RDONLY);
            if (fd < 0)
            {
                perror("open file");
                close(cfd);
                continue;
            }

            while ((len = read(fd, buf, sizeof(buf))) > 0)
            {
                send(cfd, buf, len, 0);
            }
            close(fd);
            printf("[send] ������ɣ�%s\n", filepath);
        }

        close(cfd);
    }

    return NULL;
}
