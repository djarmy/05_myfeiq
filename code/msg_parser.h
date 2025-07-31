#ifndef MSG_PARSER_H
#define MSG_PARSER_H

/**
 * @brief 结构体表示解析后的 IPMSG 报文
 */
typedef struct {
    char *version;
    char *packet_no;
    char *sender_name;
    char *host_name;
    char *command_no;
    char *extra_data;  // 可选字段
} ipmsg_packet_t;

/**
 * @brief 解析 IPMSG 报文的函数声明
 */
int parse_ipmsg_packet(const char *packet_str, ipmsg_packet_t *pkt);

#endif  // MSG_PARSER_H
