#ifndef __MSG_HANDER_H__
#define __MSG_HANDER_H__
//
#include <netinet/in.h>

#define MAX_USERNAME_LEN 32
#define MAX_HOSTNAME_LEN 64
#define MAX_MSG_LEN      512

typedef struct 
{
    char username[MAX_USERNAME_LEN];
    char hostname[MAX_HOSTNAME_LEN];
    char ipaddr[INET_ADDRSTRLEN];
}UserInfo;

//接受线程参数结构体
typedef struct 
{
    int sockfd;
    struct sockaddr_in local_addr;
}ReceiverArgs;

//线程接口函数
void* recv_msg_thread(void *arg);
void parse_and_reply(const char *buf, struct sockaddr_in *sender_addr, int sockfd);

// 向指定用户发送消息（通过编号）
void send_message_to_user(int user_id, const char *text);

void handle_received_message(const char *recv_buf);

#endif // __MSG_HANDER_H__