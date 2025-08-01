#ifndef __SYS_INFO_H__
#define __SYS_INFO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "IPMSG.H"
#include "msg_handler.h"

int sys_init(const char *user, const char *host, uint16_t port);
const char *get_user(void);
const char *get_host(void);   
int get_tcp_fd(void);
int get_udp_fd(void);
int prepare_receiver_args(ReceiverArgs *args);
void login(int sockfd, char *user, char *host);

void send_login_broadcast(void);

//定义全局互斥锁 log_mutex，包装 safe_log()，在多个线程中互斥打印： printf() 替换为 safe_log()，从根本解决并发混输的问题
void safe_log(const char *format, ...);


#endif