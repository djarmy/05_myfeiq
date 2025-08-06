#ifndef FILE_SENDER_H
#define FILE_SENDER_H

#include <sys/socket.h>
#include <netinet/in.h>

/* 
 * 功能：通过UDP发送文件通知（飞秋协议兼容）
 * 参数：
 *   ip - 目标主机IP地址字符串（如"192.168.1.100"）
 *   filepath - 待发送文件的完整路径（如"/home/user/doc.txt"）
 * 说明：
 *   1. 发送UDP通知给目标主机，告知文件信息
 *   2. 自动将文件注册到文件注册表
 *   3. 依赖get_udp_fd()获取UDP套接字
 */
void sendfile_ipmsg(const char* ip, const char* filepath);

/* 
 * 功能：TCP文件发送线程函数（需作为线程入口使用）
 * 参数：
 *   arg - 线程参数（未使用，可传NULL）
 * 返回值：
 *   线程退出状态（NULL）
 * 说明：
 *   1. 循环监听TCP连接，接收文件请求
 *   2. 根据请求中的packno和fino查找文件并发送
 *   3. 依赖get_tcp_fd()获取TCP套接字
 */
void *tcp_file_sender_thread(void *arg);

#endif  // FILE_SENDER_H
