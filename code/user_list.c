#include <string.h> 
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>

#include "msg_handler.h"


// 用户列表最大容量
#define MAX_USERS 256

// 全局用户列表缓存
static UserInfo user_list[MAX_USERS];
static int user_count = 0;

// 内部函数：检查用户是否已存在（通过 IP）
static int find_user_by_ip(const char *ipaddr) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(user_list[i].ipaddr, ipaddr) == 0) {
            return i;
        }
    }
    return -1;
}

// 添加或更新用户信息（供 parse_and_reply 调用）
void update_user_list(const char *username, const char *hostname, const char *ipaddr) 
{
    if (!username || !hostname || !ipaddr) return;

    int index = find_user_by_ip(ipaddr);

    if (index >= 0) {
        // 已存在，更新用户信息（可选）
        strncpy(user_list[index].username, username, MAX_USERNAME_LEN - 1);
        strncpy(user_list[index].hostname, hostname, MAX_HOSTNAME_LEN - 1);
        user_list[index].username[MAX_USERNAME_LEN - 1] = '\0';
        user_list[index].hostname[MAX_HOSTNAME_LEN - 1] = '\0';
    } else if (user_count < MAX_USERS) {
        // 添加新用户
        strncpy(user_list[user_count].username, username, MAX_USERNAME_LEN - 1);
        strncpy(user_list[user_count].hostname, hostname, MAX_HOSTNAME_LEN - 1);
        strncpy(user_list[user_count].ipaddr, ipaddr, INET_ADDRSTRLEN - 1);

        // 确保字符串以 '\0' 结尾
        user_list[user_count].username[MAX_USERNAME_LEN - 1] = '\0';
        user_list[user_count].hostname[MAX_HOSTNAME_LEN - 1] = '\0';
        user_list[user_count].ipaddr[INET_ADDRSTRLEN - 1] = '\0';

        user_count++;
    } else {
        fprintf(stderr, "警告：用户列表已满，无法添加新用户！\n");
    }
}

// 获取当前在线用户数量
int get_user_count(void) 
{
    return user_count;
}

// 获取指定用户信息（用于 UI 展示）
const UserInfo* get_user_info(int index) 
{
    if (index < 0 || index >= user_count) {
        return NULL;
    }
    return &user_list[index];
}
