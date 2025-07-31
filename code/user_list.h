#include <string.h>
#include <stdio.h>

#include "msg_handler.h" 


// 用户列表接口函数
void update_user_list(const char *username, const char *hostname, const char *ipaddr);
int get_user_count(void);
const UserInfo* get_user_info(int index);
