
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> 


#include "msg_handler.h"    // 消息处理模块  // 获取 get_user_count, get_user_info 等函数
#include "file_transfer.h"  // 文件发送模块
#include "IPMSG.H"          // 协议常量、命令等
#include "user_list.h"
#include "sys_info.h"
#include "file_transfer_tcp.h"

// 在包含头文件后，或在代码开头添加
#ifndef F_OK
#define F_OK 0  // 手动定义 F_OK 为 0（标准值）
#endif



// 判断输入是否为纯数字（用于选择接收方）
int is_numeric(const char *str) 
{
    if (str == NULL) return 0;
    for (int i = 0; str[i]; i++) {
        if (!isdigit((unsigned char)str[i])) return 0;
    }
    return 1;
}

// 打印在线用户列表
void print_user_list(void)
{
    int count = get_user_count();  // 获取当前在线用户数量

    // if (count == 0) {
    //     printf("\n[提示] 当前没有在线用户。\n");
    //     return;
    // }

    printf("\n当前在线用户列表：\n");
    printf("---------------------------------------------------\n");
    printf("| 编号 |     用户名     |     主机名     |   IP地址   |\n");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        const UserInfo *info = get_user_info(i);  // 获取第 i 个用户的信息
        if (info) {
            printf("|  %2d  | %-15s | %-15s | %-15s |\n",
                   i,
                   info->username,
                   info->hostname,
                   info->ipaddr);  // ip 是字符串形式
        }
    }

    printf("---------------------------------------------------\n");
}


// 终端交互主循环，负责菜单与输入处理
void ui_main_loop(void) 
{
    // 每次进入 UI 循环先检查是否有 TCP 文件提示
    const char* msg = get_last_recv_file_msg();
    if (msg) {
        printf("\n%s\n", msg);  // 打印接收端提示
    }

    char input[512];
    sleep(0.6);
    printf("\n======= 飞鸽传书（局域网通信）=======\n");
    printf("1. 查看在线用户\n");
    printf("2. 发送消息\n");
    printf("3. 发送文件\n");
    printf("4. 退出程序\n");
    safe_log("请输入选项：");
    fflush(stdout);

    // 清空输入缓冲并读取用户输入
    if (fgets(input, sizeof(input), stdin) == NULL) 
    {
        printf("输入错误，请重试。\n");
        return;
    }

    int choice = atoi(input);

    switch (choice) 
    {
        case 1:
            print_user_list(); // 显示在线用户
            break;

        case 2: 
        {
            print_user_list(); // 显示用户编号
            printf("请输入接收者编号：");
            if (fgets(input, sizeof(input), stdin) == NULL) 
            {
                printf("读取编号失败！\n");
                break;
            }
            input[strcspn(input, "\n")] = '\0';  // ✅ 去除换行符
            if (!is_numeric(input)) {
                printf("无效编号！请输入有效数字。\n");
                break;
            }

            int user_id = atoi(input);
            int user_count = get_user_count(); // 🟡 获取当前在线用户数
            if (user_id < 0 || user_id >= user_count) 
            {
                printf("无效编号！当前在线用户数量为 %d，请输入范围内编号。\n", user_count);
                break;
            }
            printf("已连接编号的用户名称为:%s, 按exit退出\n", get_user_info(user_id)->username);
            while (1)
            {
                printf("请输入要发送的消息内容：");
                if (fgets(input, sizeof(input), stdin) == NULL) {
                    printf("读取消息失败！\n");
                    break;
                }
                // 去掉末尾换行符
                input[strcspn(input, "\n")] = '\0';

                if (strlen(input) == 0) {
                    printf("不能发送空消息！\n");
                    continue;
                }
                if (strncmp(input, "exit", 4) == 0) 
                {
                    printf("已退出当前编号的聊天。\n");
                    break;  // 输入 exit 则退出消息输入循环
                }  
                send_message_to_user(user_id, input);  // 调用发送函数
            }
            
            break;
        }

        case 3: {
            print_user_list(); 
            printf("请输入接收者编号：");
            if (fgets(input, sizeof(input), stdin) == NULL) 
            {
                printf("读取编号失败！\n");
                break;
            }
            input[strcspn(input, "\n")] = '\0';  // ✅ 去除换行符
            if (!is_numeric(input)) {
                printf("无效编号！请输入有效数字。\n");
                break;
            }

            int user_id = atoi(input);
            int user_count = get_user_count();  // ✅ 获取在线用户总数
            if (user_id < 0 || user_id >= user_count) 
            {
                printf("无效编号！当前在线用户数量为 %d，请输入范围内编号。\n", user_count);
                break;
            }

            while (1)
            {
            printf("请输入文件路径(按exit退出)：");
            if (fgets(input, sizeof(input), stdin) == NULL) {
                printf("读取路径失败！\n");
                break;
            }
            // 去掉末尾换行符
            input[strcspn(input, "\n")] = '\0';

            // ✅ 检查路径是否为空
            if (strlen(input) == 0) {
                printf("[错误] 文件路径不能为空！\n");
                break;
            }

            // ✅ 检查路径长度是否超长
            if (strlen(input) >= PATH_MAX) {
                printf("[错误] 文件路径过长，不能超过 %d 字符\n", PATH_MAX - 1);
                break;
            }

            // ✅ 文件是否存在 + 是不是常规文件
            struct stat st = {0};
            if (stat(input, &st) != 0) {
                perror("[错误] 文件不存在");
                break;
            }
            if (!S_ISREG(st.st_mode)) {
                printf("[错误] 不是一个常规文件（可能是目录或特殊文件）\n");
                break;
            }

            if (strncmp(input, "exit", 4) == 0)
            {
                printf("已退出当前编号发送文件\n");
                break;
            }

            // ✅ 合法性都通过，发起发送
            printf("[调试] 正在准备发送文件: \"%s\"\n", input); 
            send_file_to_user(user_id, input);  // 调用文件发送函数
            }
            
            break;
        }

        case 4:
            printf("即将退出程序...\n");
            exit(0);

        default:
            printf("无效选项，请重新输入。\n");
    }

}
