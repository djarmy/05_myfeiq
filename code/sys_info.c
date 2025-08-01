#include <stdarg.h>
#include <stdint.h>  // 端口号、协议字段	uint16_t	固定 16 位，无符号，符合标准
//系统初始化与登录广播 文件：sys_info.c
#include "sys_info.h"
#include "uiloop.h"

static int tcp_fd = -1;
static int udp_fd = -1;
static char sys_user[32] = {0};
static char sys_host[32] = {0};

const char *get_user(void)
{
    return sys_user;
}

const char *get_host(void)
{
    return sys_host;
}

int get_tcp_fd(void)
{
    return tcp_fd;
}

int get_udp_fd(void)
{
    return udp_fd;
}

 

// 初始化 ReceiverArgs 用于传给接收线程
int prepare_receiver_args(ReceiverArgs *args)
{
    if (!args) return -1;

    int udp_sockfd = get_udp_fd();                  //返回已初始化的 UDP 套接字
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);

    if (getsockname(udp_sockfd, (struct sockaddr *)&local_addr, &len) < 0)
    {
        perror("getsockname failed");
        return -1;
    }

    args->sockfd = udp_sockfd;       // 赋值本地地址
    args->local_addr = local_addr;   // 赋值本地地址
    return 0;
}

void send_login_broadcast(void)    //int sockfd, char *user, char *host
{
   char buf[256] = {0};

    struct sockaddr_in bcast_addr;
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(IPMSG_DEFAULT_PORT);
    bcast_addr.sin_addr.s_addr = inet_addr("192.168.3.255");    //前者子网定向广播可避免对其它子网造成干扰;后者是inet_addr("255.255.255.255");全局地址广播，可能对无关设备造成流量干扰。

    // 合并飞秋协议    ;规范写法应使用 %lu，明确匹配 unsigned long 类型
    snprintf(buf, sizeof(buf)-1, "1:%ld:%s:%s:%lu:%s", time(NULL), get_user(), get_host(), IPMSG_BR_ENTRY, get_user());    //避免潜在误解：新手可能不清楚 snprintf 会自动留终止符，显式减 1 能减少理解成本。
     
    // 发送登录协议
    if(sendto(udp_fd, buf, strlen(buf), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr)) < 0)
    {
        perror("sendto failed");
        exit(-1);
    }
}


    // 系统初始化环境，包括用户名/主机名、UDP/TCP 套接字等
    /**
     * @brief 系统初始化（设置用户名、主机名、创建套接字、绑定、发送上线广播）
     *
     * @param user 用户名
     * @param host 主机名
     * @return int 初始化成功返回0，失败返回-1
     */
int sys_init(const char *user, const char *host, uint16_t port)
{
    if (!user || !host)
    {
        return -1;
    }

    strncpy(sys_user, user,sizeof(sys_user)-1);    //无论源字符串const char * 多长，我只复制dest能够容纳的最大实际字符数=（字节数-1个终止符）。
    strncpy(sys_host, host,sizeof(sys_host)-1);   
    
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    addr.sin_family = AF_INET; // ipv4
    addr.sin_port = htons(port);    //2425   IPMSG_DEFAULT_PORT
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    strcpy(sys_user, user);
    strcpy(sys_host, host);

    // 创建原始udp套接字
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_fd < 0)
    {
        perror("socket failed udp_fd");
        exit(-1);
    }

    // 绑定udp套接字
    if(bind(udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        close(udp_fd);
        exit(-1);
    }
    // 设置广播属性
    int opt = 1;
    if(setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        close(udp_fd);
        exit(-1);
    }

    // 创建tcp套接字
    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_fd < 0)
    {
        perror("socket failed tcp_fd");
        close(udp_fd);
        exit(-1);
    }
    // 绑定tcp套接字
    if(bind(tcp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed tcp_fd");
        close(udp_fd);
        close(tcp_fd);
        exit(-1);
    }
    // 监听tcp套接字
    if(listen(tcp_fd, 10) < 0)
    {
        perror("listen failed tcp_fd");
        close(udp_fd);
        close(tcp_fd);
        exit(-1);
    }

    // feiq登录 发送上线广播
    send_login_broadcast();    //(udp_fd, user, host)
    sleep(2);  // 等待其他主机回应
    //print_user_list();  // 查看回应结果（调试用）   
    return 0;
}




pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
//定义全局互斥锁 log_mutex，包装 safe_log()，在多个线程中互斥打印： printf() 替换为 safe_log()，从根本解决并发混输的问题
void safe_log(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    pthread_mutex_lock(&log_mutex);
    vfprintf(stdout, format, args);
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
    va_end(args);
}
