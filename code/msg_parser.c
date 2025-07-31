#include "msg_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 解析 IPMSG 报文，将其分解为结构体字段
 * @param buf 原始报文字符串
 * @param packet 结果结构体指针，用于保存解析结果
 * @return 0 表示成功，-1 表示失败
 */
int parse_ipmsg_packet(const char *buf, ipmsg_packet_t *packet) {
    if (!buf || !packet) return -1;

    // 初始化字段为空，防止误用
    memset(packet, 0, sizeof(ipmsg_packet_t));

    char *copy = strdup(buf);
    if (!copy) return -1;

    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);

    char *fields[6] = {0};
    int i = 0;

    while (token && i < 6) {
        fields[i++] = token;
        token = strtok_r(NULL, ":", &saveptr);
    }

    if (i < 5) {
        fprintf(stderr, "[错误] 报文字段不足，仅解析到 %d 个字段\n", i);
        free(copy);
        return -1;
    }

    // extra_data 保留所有剩余内容（第6个字段起）
    const char *extra_data = saveptr;

    // 字段赋值，失败即回滚
    packet->version     = fields[0] ? strdup(fields[0])     : NULL;
    packet->packet_no   = fields[1] ? strdup(fields[1])     : NULL;
    packet->sender_name = fields[2] ? strdup(fields[2])     : NULL;
    packet->host_name   = fields[3] ? strdup(fields[3])     : NULL;
    packet->command_no  = fields[4] ? strdup(fields[4])     : NULL;
    packet->extra_data  = extra_data ? strdup(extra_data)   : NULL;

    // 检查是否有任一字段为空（防御性）
    if (!packet->version || !packet->packet_no || !packet->sender_name ||
        !packet->host_name || !packet->command_no) {

        fprintf(stderr, "[错误] 报文字段缺失：version=%p, pkt=%p, name=%p, host=%p, cmd=%p\n",
                packet->version, packet->packet_no, packet->sender_name,
                packet->host_name, packet->command_no);

        // 清理已分配的字段
        if (packet->version)     free(packet->version);
        if (packet->packet_no)   free(packet->packet_no);
        if (packet->sender_name) free(packet->sender_name);
        if (packet->host_name)   free(packet->host_name);
        if (packet->command_no)  free(packet->command_no);
        if (packet->extra_data)  free(packet->extra_data);

        free(copy);
        return -1;
    }

    free(copy);
    return 0;
}
