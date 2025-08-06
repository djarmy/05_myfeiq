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

    memset(packet, 0, sizeof(ipmsg_packet_t));
    char *copy = strdup(buf);
    if (!copy) return -1;

    char *parse_start = copy;

    // =========== 🧠 判断是否为飞秋扩展广播格式（前缀中含有 #） ============
    if (strchr(copy, '#')) {
        // 🔍 找第一个冒号（扩展格式前缀与 IPMSG 正文之间用冒号分隔）
        char *first_colon = strchr(copy, ':');
        if (!first_colon) {
            fprintf(stderr, "[错误] 扩展格式报文缺少冒号，非法格式: %s\n", buf);
            free(copy);
            return -1;
        }

        // 截断扩展前缀
        *first_colon = '\0';

        // 设置 version 字段为标准值 "1"
        packet->version = strdup("1");

        // 继续解析剩下的字段（冒号后部分）
        parse_start = first_colon + 1;
    } else {
        parse_start = copy;
    }

    // =========== 🧠 开始标准字段解析（从 parse_start 继续） ============

    char *temp[20] = {0}; 
    int offset = 0;
    temp[offset++] = strtok(parse_start, ":");
    while (offset < 20 && (temp[offset++] = strtok(NULL, ":")) != NULL);
    int field_count = offset - 1;
 
    // 报文字段必须 ≥ 5（version, packet_no, sender_name, host_name, command_no）
    if (field_count < 4) {
        fprintf(stderr, "[错误] 报文字段不足，仅解析到 %d 个字段\n", field_count);
        free(copy);
        return -1;
    }

    // 计算字段偏移（扩展格式无 version，非扩展格式 version 占 temp[0]）
    int base = 0;
    if (!packet->version) 
    {
        packet->version = strdup(temp[0]);
        if (!packet->version || strcmp(packet->version, "1") != 0) {
            fprintf(stderr, "[错误] 不支持的协议版本: %s（仅支持 version=1）\n", packet->version);
            free(copy);
            return -1;
        }
        base = 1;  // 非扩展格式，字段从 temp[1] 开始
    } else {
        base = 0;  // 扩展格式已处理 version，字段从 temp[0] 开始
    }
    
    // 防御性检查剩余字段数量是否足够
    if (field_count < base + 4) {
        fprintf(stderr, "[错误] 报文字段不足（需要至少 %d 个字段）\n", base + 4);
        free(copy);
        return -1;
    }

    packet->packet_no   = strdup(temp[base + 0]);
    packet->sender_name = strdup(temp[base + 1]);
    packet->host_name   = strdup(temp[base + 2]);
    packet->command_no  = strdup(temp[base + 3]);


    // extra 字段从 base+4 开始拼接
    if (field_count > base + 4) {
        size_t extra_len = 0;
        for (int j = base + 4; j < field_count; j++) {
            extra_len += strlen(temp[j]) + 1;
        }

        char *extra = malloc(extra_len + 1);
        if (!extra) {
            free(copy);
            return -1;
        }

        extra[0] = '\0';
        for (int j = base + 4; j < field_count; j++) {
            strcat(extra, temp[j]);
            if (j < field_count - 1) strcat(extra, ":");
        }
        packet->extra_data = extra;
    } else {
        packet->extra_data = strdup("");
    }

    // printf("[调试] version=%s, packet_no=%s, sender_name=%s, host_name=%s, command_no=%s, extra=%s\n",
    //        packet->version, packet->packet_no, packet->sender_name,
    //        packet->host_name, packet->command_no, packet->extra_data);

    free(copy);
    return 0;
}
