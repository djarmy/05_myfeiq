#include <stdio.h> 
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>  // 包含 strtoul()
#include <errno.h>   // 包含 errno 定义
#include <limits.h>  // 包含 ULONG_MAX 定义
#include <stdbool.h>

#include "msg_handler.h"
#include "IPMSG.H"  // 使用你项目中提供的协议头文件
#include "user_list.h"
#include "msg_parser.h"
#include "sys_info.h"   // 新增，获取 get_user/get_host
#include "file_registry.h"


// 全局变量（全文件共享）
char username[MAX_USERNAME_LEN];            
char hostname[MAX_HOSTNAME_LEN];
// 初始化标志
static bool info_initialized = false;
// 初始化 username 和 hostname，保证只执行一次
static void init_user_info_once() {
    if (!info_initialized) {
        strncpy(username, get_user(), sizeof(username) - 1);
        strncpy(hostname, get_host(), sizeof(hostname) - 1);
        username[sizeof(username) - 1] = '\0';
        hostname[sizeof(hostname) - 1] = '\0';
        info_initialized = true;
    }
}


static char last_packet_no[64] = {0};         

static unsigned long get_packet_no()
{
    // return (unsigned long)time(NULL);
    static unsigned long packet_no = 0;  // 从0开始
    return ++packet_no;  // 每次调用返回递增的编号
}

//处理上线广播或者消息
void parse_and_reply(const char *buf, struct sockaddr_in *sender_addr, int sockfd)     
{
    if (!buf || !sender_addr)
    {
        return;
    }
 
    ipmsg_packet_t pkt;
    if (parse_ipmsg_packet(buf, &pkt) != 0) {
        fprintf(stderr, "[错误] 解析报文失败，已忽略该报文。\n");
        return;
    }

    const char* version     = pkt.version;
    const char* packet_no   = pkt.packet_no;
    const char* sender_name = pkt.sender_name;
    const char* sender_host = pkt.host_name;
    const char* cmd_str     = pkt.command_no;
    const char* extra       = pkt.extra_data;

    if (!version || !packet_no || !sender_name || !sender_host || !cmd_str || !extra)
    {
        perror("exist Null pointer");
        fprintf(stderr, "[错误] 报文字段缺失：version=%p, pkt=%p, name=%p, host=%p, cmd=%p, extra=%p\n",
                version, packet_no, sender_name, sender_host, cmd_str, extra);
        goto cleanup;
    }
    //fprintf(stderr, "[调试]收到报文长度: %ld 字节。 原始报文内容: [%s]\n", strlen(buf),buf);

    unsigned int cmd = 0;
    unsigned int main_cmd = 0;                                                                      
    // 防御性处理：检查 cmd_str 是否为空
    if (cmd_str == NULL || *cmd_str == '\0') {
        // 处理空字符串错误（如日志、返回错误码）
        fprintf(stderr, "错误：命令字符串为空\n");
        return;  // 或其他错误处理逻辑
    }

    char *endptr;
    errno = 0;  // 重置 errno，用于检测范围错误
    unsigned long cmd_ulong = strtoul(cmd_str, &endptr, 10);

    // 检查转换结果：是否有有效数字，是否完全转换
    if (endptr == cmd_str) {
        // 无有效数字（如 "abc123" 中 "abc" 不是数字）
        fprintf(stderr, "错误：命令字符串格式无效\n");
        return;
    }
    if (*endptr != '\0') {
        // 字符串未完全转换（如 "123abc" 中 "abc" 未被转换）
        fprintf(stderr, "警告：命令字符串包含额外字符，已忽略\n");
    }

    // 检查是否超出范围
    if (cmd_ulong == ULONG_MAX && errno == ERANGE) {
        fprintf(stderr, "错误：命令值超出范围\n");
        return;
    }

    // 检查是否超出 unsigned int 范围（若系统中 long 比 int 长）
    if (cmd_ulong > UINT_MAX) {
        fprintf(stderr, "错误：命令值超出 unsigned int 范围\n");
        return;
    }

    // 安全转换为 unsigned int
    cmd = (unsigned int)cmd_ulong;

    // 提取主命令（低 8 位）
    main_cmd = cmd & 0xFF;  // 0x000000ff 可简化为 0xFF，效果相同

    // 将用户加入在线列表（用于 UI 展示）
    update_user_list(sender_name, sender_host, inet_ntoa(sender_addr->sin_addr));      //无论收到 IPMSG_BR_ENTRY、IPMSG_SENDMSG 还是其他命令，只要来源主机有效就能更新用户列表。

    if (main_cmd == IPMSG_BR_ENTRY)
    {
        //构造应答IPMSG_ANSENTRY  我收到对方上线广播 → 我要回个 IPMSG_ANSENTRY 表示我在线，如果你 不回包，对方根本 不会知道你上线了。
        char msg[512] = {0};

        unsigned long pkt_no = get_packet_no();
        snprintf(msg, sizeof(msg), "1:%lu:%s:%s:%lu:%s",
                pkt_no,                    // 报文编号
                get_user(),                  // 本机用户名改成函数调用
                get_host(),                  // 本机主机名
                (unsigned long)IPMSG_ANSENTRY,  // 主命令
                get_user());                 // ✅ 附加字段必须有
        //fprintf(stderr, "[调试]回应上线应答报文(IPMSG_ANSENTRY): %s\n", msg);
        if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*)sender_addr, sizeof(*sender_addr)) < 0)    //来自第二个参数结构体变量指针    . 6个参数
        {
            perror("sendto failed");
        }
    }else if (main_cmd == IPMSG_SENDMSG)
    {
        if (strcmp(packet_no, last_packet_no) == 0) {
            return;  // 已处理过此条消息
        }
        strncpy(last_packet_no, packet_no, sizeof(last_packet_no) - 1);
        last_packet_no[sizeof(last_packet_no) - 1] = '\0';

        printf("\n收到来自%s@%s的消息：%s\n", sender_name, sender_host, extra ? extra : "");    // 如果 extra 不是 NULL，就用 extra 的值   ,三目运算符简洁性：用一行代码替代 if-else 块 
        //发送RECVMSG应答
        char ack_msg[256] = {0}; 
        unsigned long pkt_no = get_packet_no();
        snprintf(ack_msg, sizeof(ack_msg), "1:%lu:%s:%s:%lu:%s", pkt_no, username, hostname, IPMSG_RECVMSG, packet_no);    // 接受消息#define IPMSG_RECVMSG			0x00000021UL
        if (sendto(sockfd, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr)) < 0)    //当 flags 为 0 时，表示不设置任何特殊标志，使用系统默认的发送模式，
        {
            perror("sendto failed ack_msg");
        }
    }else if (main_cmd == IPMSG_ANSENTRY)
    {
        //收到对方上线回应，更新在线用户列表  
        update_user_list(sender_name, sender_host, inet_ntoa(sender_addr->sin_addr));
    }

    //【增量修改点】对文件传输类报文的解析
    if ((cmd & IPMSG_SENDMSG) && (cmd & IPMSG_FILEATTACHOPT))
    {
        printf("收到来自[%s:%s]的文件发送请求\n", sender_name, sender_host);
        // 文件信息以 \a 开头：即文件部分字段和消息内容之间用 \a 分隔
        // extra 格式可能类似于： "hello\a123456:filename.txt:12345678:0:0:77"
        
        char *msg_body = NULL;
        char *file_info = NULL;

        //复制extra内容，防止修改原始数据
        char extra_copy[MAX_MSG_LEN] = {0};
        strncpy(extra_copy, extra, sizeof(extra_copy) - 1); 
        extra_copy[sizeof(extra_copy) - 1] = '\0';

        //第一次 \a 之前是文本消息内容，之后是文件字段
        msg_body = strtok(extra_copy, "\a");    //文本消息
        file_info = strtok(NULL, "\a");         //文件字段 因此返回 123456:filename.txt:12345678:0:0:77

        if (msg_body)
        {
            printf("消息正文：%s\n", msg_body);
        }

        if (file_info)
        {
            printf("文件信息字段：%s\n", file_info);

            // 多个文件用 '\a' 分隔，所以用循环
            while (file_info)
            {
                // 文件字段格式: 123456:filename.txt:12345678:0:0:77
                char *file_saveptr = NULL;
                char *file_id_str = strtok_r(file_info, ":", &file_saveptr);
                char *file_name = strtok_r(NULL, ":", &file_saveptr);
                char *file_size_str = strtok_r(NULL, ":", &file_saveptr);
                // 后续字段可忽略或扩展（modtime, attr, extend attr）

                if (file_id_str && file_name && file_size_str)
                {
                    unsigned long file_id = strtoul(file_id_str, NULL, 10);
                    unsigned long file_size = strtoul(file_size_str, NULL, 10);

                    printf("文件名: %s\n", file_name);
                    printf("文件ID: %lu\n", file_id);
                    printf("文件大小: %lu 字节\n", file_size);

                    // 在此处调用 file_transfer 接口进行进一步处理（比如请求文件、建立连接等）
                    // 例如：handle_incoming_file(file_id, file_name, file_size, sender_addr);
                }
                else
                {
                    fprintf(stderr, "警告：文件信息字段解析失败\n");
                }

                // 继续解析下一个 \a 分隔的文件信息
                file_info = strtok(NULL, "\a");
            }
        }
        else
        {
            fprintf(stderr, "未检测到合法的文件字段\n");
        }
         
    }
    cleanup:
    free(pkt.version);
    free(pkt.packet_no);
    free(pkt.sender_name);
    free(pkt.host_name);
    free(pkt.command_no);
    if (pkt.extra_data) free(pkt.extra_data);
}



//接受线程
void* recv_msg_thread(void *arg)
{
    ReceiverArgs* args = (ReceiverArgs *)arg;    //接受者线程2个成员的结构体   // 转换（即使 arg 为 NULL，也合法）
    if (!args)                                      //对 arg 间接的检查！因为 args 来自 arg 的转换
    {
        perror("The forced conversion failed");
        return NULL;
    }
    
    char buf[MAX_MSG_LEN] = {0};
    struct sockaddr_in sender_addr = {0};
    socklen_t sender_addr_len = sizeof(sender_addr);

    while (1)
    {
        memset(buf, 0, sizeof(buf));
        ssize_t recvlen = recvfrom(args->sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (recvlen > 0)
        {
            buf[recvlen] = '\0';
            //fprintf(stderr, "\n[接收线程] 处理收到的报文...\n");
            parse_and_reply(buf, &sender_addr, args->sockfd);     //处理收到的报文
        }else{perror("recvfrom failed");}
        
    }
    return NULL;
}


// 发送消息给指定用户
void send_message_to_user(int user_id, const char *text)
{
    init_user_info_once();  // ⭐️ 添加这句，确保 username 和 hostname 是有效值
    if (!text || strlen(text) == 0) {
        printf("[错误] 消息内容为空，发送失败！\n");
        return;
    }

    const UserInfo *info = get_user_info(user_id);  // 获取用户信息
    if (!info) {
        printf("[错误] 用户编号无效，发送失败！\n");
        return;
    }

    // 创建目标地址结构体
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(2425);  // 飞鸽默认端口
    if (inet_pton(AF_INET, info->ipaddr, &target_addr.sin_addr) <= 0) {
        perror("[错误] IP 地址格式无效");
        return;
    }

    // 构造协议数据
    char msg_buf[MAX_MSG_LEN] = {0};
    unsigned long pkt_no = get_packet_no();
    unsigned long cmd = IPMSG_SENDMSG | IPMSG_SENDCHECKOPT;  // 发送消息命令 这样飞秋客户端才能正确识别为 “带确认”的消息，并在窗口中显示。

    snprintf(msg_buf, sizeof(msg_buf), "1:%lu:%s:%s:%lu:%s",
             pkt_no, username, hostname, cmd, text);

    //  使用主 socket
    int sockfd =  get_udp_fd();  // 使用已绑定 2425 端口的全局 socket
    if (sockfd < 0) {
        perror("创建 socket 失败");
        return;
    }

    ssize_t sent = sendto(sockfd, msg_buf, strlen(msg_buf), 0,
                          (struct sockaddr *)&target_addr, sizeof(target_addr));
    if (sent < 0) {
        perror("消息发送失败");
    } else {
        //printf("[成功] 已发送消息给 %s@%s (%s)\n",info->username, info->hostname, info->ipaddr);
    }

     
}


//处理接受的消息
void handle_received_message(const char *recv_buf) 
{
    ipmsg_packet_t pkt;

    if (parse_ipmsg_packet(recv_buf, &pkt) != 0) {
        fprintf(stderr, "[错误] 解析报文失败，已忽略该报文。\n");
        return;
    }

    // 使用结构体中的字段替代 strtok 结果
    const char* version     = pkt.version;
    const char* packet_no   = pkt.packet_no;
    const char* sender_name = pkt.sender_name;
    const char* sender_host = pkt.host_name;
    const char* cmd_str     = pkt.command_no;
    const char* extra       = pkt.extra_data;   

    // 可以安全使用 pkt 的各字段
    printf("[解析成功] 用户 %s (%s) 发送命令：%s\n",
           pkt.sender_name, pkt.host_name, pkt.command_no);

    // 使用完后释放内存
    free(pkt.version);
    free(pkt.packet_no);
    free(pkt.sender_name);
    free(pkt.host_name);
    free(pkt.command_no);
    if (pkt.extra_data) free(pkt.extra_data);
}
