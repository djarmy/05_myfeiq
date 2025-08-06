#include <stdio.h>             // printf, perror
#include <stdlib.h>            // malloc, free
#include <string.h>            // memset, strlen
#include <unistd.h>            // read, close
#include <fcntl.h>             // open
#include <arpa/inet.h>         // inet_ntoa
#include <pthread.h>           // pthread_t, pthread_create
#include <sys/socket.h>        // socket, sendto
#include <sys/stat.h>          // struct 
#include <limits.h>  // 包含 PATH_MAX 定义
#include <errno.h> 
#include <limits.h>
#include <time.h>
 
#include "file_transfer.h"     // 引入头文件
#include "msg_handler.h"
#include "user_list.h"
#include "file_transfer_tcp.h"  // ? 引入 TCP 文件发送头文件
#include "sys_info.h" 
#include "file_registry.h"

//线程句柄进行传输文件
pthread_t file_send_thread = {0};

//UDP套接字进行传输发送 文件数据
static int file_socket_fd = -1;



//    文件传输初始化函数(例如初始化UDP套接字)
void init_file_transfer(void)
{
    file_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);                //
    if (file_socket_fd < 0)
    {
        perror("[file_transfer]socket failed");
        exit(EXIT_FAILURE);             //无法继续工作直接退出
    }
}


//安全清理释放文件传输资源（如关闭socket）
void cleanup_file_transfer(void)
{
    if (file_socket_fd != -1)
    {
        close(file_socket_fd);
        file_socket_fd = -1;    //重置句柄
    }
}

//          文件发送线程函数指针逻辑
static void *file_send_thread_func(void* args);

// “接口在前，实现在后” 的方式更适合多人协作或长期维护的代码。
//启动发送文件线程(目标绝对路径，目标接受地址)
bool start_file_send(const struct sockaddr_in *dest_addr, const char *file_path)
{
    // 原检查：非空和非空字符串
    if (!dest_addr || !file_path || *file_path == '\0') {  // 简化 strlen == 0 为 *file_path == '\0'
        fprintf(stderr, "[file_transfer] 目标地址或文件路径为空\n");
        return false;
    }

    // 文件存在性检测（是否可读）
    if (access(file_path, R_OK) != 0) {
        fprintf(stderr, "[start_file_send] 文件无法访问或不存在: %s，错误原因: %s\n", file_path, strerror(errno));
        return false;
    }

    // 新增：检查路径长度是否超过缓冲区（避免拷贝时截断）
    if (strlen(file_path) >= sizeof( ((file_send_args_t*)0)->file_path ) -1)        //将空指针 0 强制转换为 file_send_args_t* 类型的指针检查字符串长度是否超过数组容量
    {  // 假设结构体可见
        fprintf(stderr, "[file_transfer] 文件路径过长（最大%d字节）\n", 
                (int)sizeof(*file_path) - 1);
        return false;
    }

    // 新增：检查路径是否包含危险字符（如Windows下的 \/:*?"<>|，视系统而定）
    const char *invalid_chars = ":*?\"<>|";  // Windows下的非法文件名字符
    if (strpbrk(file_path, invalid_chars) != NULL) {
        fprintf(stderr, "[file_transfer] 文件路径包含非法字符\n");
        return false;               //参数检查失败
    }

    //分配 文件数据：文件数据传输的参数结构体
    file_send_args_t *args = malloc(sizeof(file_send_args_t));
    if (!args)
    {
        perror("[file_transfer] malloc failed");
        return false;
    }
    
    memset(args, 0, sizeof(file_send_args_t));   //清空内存0
    memcpy(&args->dest_addr, dest_addr, sizeof(struct sockaddr_in));     //从第一个形参拷贝了目标地址给结构体args指针的成员
    strncpy(args->file_path, file_path, sizeof(args->file_path) - 1);    //拷贝路径 


    int ret = pthread_create(&file_send_thread, NULL, file_send_thread_func, args);    //第一个参数就是线程执行者
    if (ret != 0)
    {
        perror("[file_transfer] pthread_create failed");
        free(args);             //第四个参数通常是堆空间malloc 动态分配
        return false;
    }
    printf("成功启动:文件发送线程函数指针逻辑\n");
    // 可选：线程分离
    pthread_detach(file_send_thread);
    return true;                
}


//          文件发送线程函数指针逻辑
static void *file_send_thread_func(void* args)
{
    file_send_args_t *file_args = (file_send_args_t *)args;              //数据类型转换
    char file_path[PATH_MAX] = {0};
    strncpy(file_path, file_args->file_path, sizeof(file_path) - 1);                     //获取第二个成员文件路径
    struct sockaddr_in dest_addr = file_args->dest_addr;                       // 获取目标地址
    free(file_args);                                                      //都是指向同一个虚拟内存地址的起始地址，释放参数结构体args 。释放前拷贝一份给file_path

    printf("[调试] 尝试打开文件路径: \"%s\"\n", file_path);
    int fd = open(file_path, O_RDONLY);                //以只读形式打开
    if (fd < 0)
    {
        fprintf(stderr, "[file_transfer] open failed for path: %s, errno=%d, reason=%s\n",
        file_path, errno, strerror(errno));  // ?? 更清晰错误打印                                                    
        pthread_exit(NULL);             //第一个参数就是线程执行者 退出     。退出当前正在执行的线程。精准终止 file_send_thread 线程，无需额外指定(当 file_send_thread_func 函数被新线程执行时就关联了上下文)。
    }
    printf("[file_transfer] open 成功，fd=%d\n", fd);
    struct stat st = {0};
    //获取打开的文件 大小
    if (fstat(fd, &st) < 0)
    {
        perror("[file_transfer]fstat failed");
        close(fd);
        pthread_exit(NULL);     //打开文件失败，终止当前线程，无返回值
    }
    
    size_t total_size = st.st_size;         //文件总大小
    size_t sent_size = 0;                   //已经发送字节数大小
    char buffer[FILE_CHUNK_SIZE] = {0};           //单个文件块缓冲区大小

    // ? 发送头部信息，包含文件名与大小
    const char *filename = strrchr(file_path, '/');
    filename = (filename != NULL) ? filename + 1 : file_path; // 取最后一个 / 后的文件名
    char header_buf[PATH_MAX + 64] = {0};
    snprintf(header_buf, sizeof(header_buf), "HEADER:%s:%zu\n", filename, total_size);

    ssize_t send_len = sendto(file_socket_fd, header_buf, sizeof(header_buf), 0,
                    (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (send_len == -1) {
        // 详细错误信息（通过errno获取具体具体）
        perror("sendto失败"); // 自动动输出errno对应的错误描述（如"Connection refused"）
        return NULL;
    }
    usleep(10000);  // ?? 稍作等待，防止第一包丢失 

    while (sent_size < total_size)             //这正是 “分块读取 + 发送” 的设计目的
    {
        if (total_size == 0) 
        {
        fprintf(stderr, "[file_transfer] 警告：尝试发送空文件 \"%s\"，操作已跳过。\n", file_path);
        close(fd);
        pthread_exit(NULL);
        }

        //读取一块数据
        ssize_t read_bytes = read(fd, buffer, FILE_CHUNK_SIZE);  
        printf("[DEBUG] read() 返回: %zd\n", read_bytes);
          
        //优先处理读取到文件末尾。read() 返回 -1 表示错误，0 表示 EOF，>0 表示成功读取，三者互不重叠，逻辑清晰。
        if (read_bytes == 0)
        {
            if (sent_size < total_size)
            {
                fprintf(stderr,"[%s:%d]警告：文件实际大小数值小于预期(已经发送：%zu)\n", __FILE__, __LINE__, sent_size);
            }
            break;      //必须退出，否则无限循环
        }
        
        if (read_bytes < 0) 
        {
            fprintf(stderr, "[%s:%d] file_transfer failed\n", __FILE__, __LINE__);
            perror("[file_trasnfer]");
            break;                          //出错跳出循环
        }
        
        ssize_t sent_bytes = sendto(file_socket_fd, buffer, read_bytes, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));          //发送一块数据大小             6个参数
        if (sent_bytes < 0)
        {
            fprintf(stderr, "[%s:%d]文件数据发送失败", __FILE__, __LINE__);
            perror("sendto failed");
            break;                          //出错跳出循环
        }
        sent_size += sent_bytes;            //累计已发送总字节数大小
        printf("[调试] 是否已经成功发送一个字节\n");
    }
    // ? 循环结束后再发送 END
    const char *end_msg = "END\n";
    ssize_t end_sent = sendto(file_socket_fd, end_msg, strlen(end_msg), 0,
                            (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (end_sent < 0) {
        perror("[file_transfer] sendto END失败");
    } else {
        printf("[file_transfer] 已发送文件结束标志 END\n");
    }

    // ? 发送头部信息，包含文件名与大小
    filename = strrchr(file_path, '/');
    filename = (filename != NULL) ? filename + 1 : file_path; // 取最后一个 / 后的文件名
     
    snprintf(header_buf, sizeof(header_buf), "HEADER:%s:%zu\n", filename, total_size);

    sendto(file_socket_fd, header_buf, strlen(header_buf), 0,
           (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    usleep(10000);  // ?? 稍作等待，防止第一包丢失
    printf("[调试] 累计已发送总字节数大小：%d\n", (int)sent_size);
    sendto(file_socket_fd, "ACK", 3, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    pthread_exit(NULL);
}



// === 新增静态函数，用于获取当前时间戳字符串 ===
static void get_timestamp_str(char *buf, size_t len) {
    time_t t = time(NULL);
    snprintf(buf, len, "%ld", t);
}

// === 新增函数：模仿飞秋协议的 IPMSG 文件发送通告 ===
static void send_ipmsg_file_announce(const struct sockaddr_in *dest_addr, const char *username, const char *hostname, const char *filepath, const char *file_id) 
{
    if (!dest_addr || !username || !hostname || !filepath) 
    {
    fprintf(stderr, "[IPMSG] 参数无效，无法发送文件通告\n");
    return;
    }

    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("[IPMSG] 无法获取文件信息");
        return;
    }    
        char buf[1024] = {0};       // 主报文
    char fileopt[512] = {0};    // 附加字段

    // 构造 fileopt（飞秋标准格式）
    snprintf(fileopt, sizeof(fileopt),
             "%s:%s:%ld:%ld:%lu:",
             file_id,
             filename,
             (long)st.st_size,
             (long)st.st_mtime,
             (unsigned long)IPMSG_FILE_REGULAR);

    // 构造主报文前五段
    unsigned long packet_no = (unsigned long)time(NULL);
    unsigned long command = IPMSG_SENDMSG | IPMSG_FILEATTACHOPT;
    int msg_len = snprintf(buf, sizeof(buf),
                           "%u:%lu:%s:%s:%lu:",
                           IPMSG_VERSION, packet_no,
                           username, hostname,
                           command);

    if (msg_len <= 0 || msg_len >= sizeof(buf)) 
    {
        fprintf(stderr, "[IPMSG] 构造主报文失败，长度溢出或错误\n");
        return;
    }

    // 添加 \0 + fileopt 到 buf 后部
    size_t buf_len = strlen(buf);
    size_t fileopt_len = strlen(fileopt);
    memcpy(buf + buf_len + 1, fileopt, fileopt_len);

    // 注意这里用 total_len，不再是 strlen(buf)
    ssize_t total_len = buf_len + 1 + fileopt_len;

    ssize_t sent = sendto(file_socket_fd, buf, total_len, 0,
                          (struct sockaddr *)dest_addr, sizeof(*dest_addr));

    if (sent == -1) {
        perror("[IPMSG] 文件通告发送失败");
    } else {
        printf("[IPMSG] 成功发送文件通告（兼容飞秋格式），等待对方拉取：\n>> %s\n", buf);
    }
}



// 从用户编号和文件路径，发送文件
void send_file_to_user(int user_id, const char *filepath)
{
    if (!filepath || strlen(filepath) == 0) {
        printf("[错误] 文件路径为空，发送失败！\n");
        return;
    }

    int file_id = generate_file_id();
    char file_id_str[32];  // 存储字符串形式的file_id
    snprintf(file_id_str, sizeof(file_id_str), "%d", file_id);  // 转换为字符串

    const UserInfo *info = get_user_info(user_id);  // 获取用户信息
    if (!info) {
        printf("[错误] 用户编号无效，发送失败！\n");
        return;
    }
 
    
    // 设定 TCP 文件传输端口（客户端连接接收方）
    const uint16_t FILE_TCP_PORT = 2499;

    printf("[提示] 正在通过 TCP 发送文件 \"%s\" 给 %s@%s [%s] ...\n",
           filepath, info->username, info->hostname, info->ipaddr);

    //  使用 TCP 实现文件发送（新逻辑）失败则使用UDP
    // 增加调用 TCP 文件传输前的网络可达性检测
    struct sockaddr_in dest_tcp = {0};
    dest_tcp.sin_family = AF_INET;
    dest_tcp.sin_port = htons(FILE_TCP_PORT);
    inet_pton(AF_INET, info->ipaddr, &dest_tcp.sin_addr);

    // 检查是否能连通目标主机的 TCP 文件端口
    int testfd = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval tv = {3, 0};  // 3秒超时
    setsockopt(testfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(testfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(testfd, (struct sockaddr*)&dest_tcp, sizeof(dest_tcp)) < 0) 
    {
        perror("[警告] 无法连接目标 TCP 文件端口，尝试 UDP");
        close(testfd);
        // fallback to UDP
        struct sockaddr_in dest_addr = {0};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(FILE_UDP_PORT);      //UDP通信协议 
        if (inet_pton(AF_INET, info->ipaddr, &dest_addr.sin_addr) <= 0)               // “presentation”（展示）
        {
            fprintf(stderr, "[%s:%d] 错误，IP地址格式无效，无法启动UDP通信协议传输\n", __FILE__, __LINE__);
            return;
        }
        // ? 添加 IPMSG 协议兼容广播
        send_ipmsg_file_announce(&dest_addr, get_user(), get_host(), filepath, file_id_str);

        unsigned long packet_no = generate_file_id();  //  
        // 在 send_ipmsg_file_announce() 之后添加：
        char packno_str[32];
        snprintf(packno_str, sizeof(packno_str), "%lu", packet_no);

        register_file(packno_str, file_id_str, filepath);
        printf("[注册] 已登记 packno=%s, fino=%s, path=%s\n", packno_str, file_id_str, filepath);


        if (!start_file_send(&dest_addr, filepath))
        {
            fprintf(stderr, "[%s:%d] 错误，UDP 文件发送也失败，放弃发送。\n", __FILE__, __LINE__);
        }else {
            printf("[提示] 已启动 UDP 文件发送线程（成功与否将在后台日志中显示）...\n");
        }
    } else {
        close(testfd);
        // ? 启动 TCP 发送线程
        if (!start_tcp_file_send(info->ipaddr, FILE_TCP_PORT, filepath)) {    
            fprintf(stderr, "[警告] start_tcp_file_send 返回失败，尝试 UDP\n");
            // fallback to UDP
        } else {
            printf("[提示] 已启动 TCP 文件发送线程\n");
            return;
        }
    } 
}



// #define FILE_RECV_PORT 2425    // 确保发送和接收一致！
// #define FILE_BUFFER_SIZE 4096

static void* udp_file_receiver_thread(void *arg)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("[recv] socket");
        pthread_exit(NULL);
    }
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));  //   关键代码

    struct sockaddr_in local_addr = {0};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(FILE_RECV_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        perror("[recv] bind");
        close(sockfd);
        pthread_exit(NULL);
    }

    //printf("[recv] UDP 接收线程启动，监听端口 %d\n", FILE_RECV_PORT);

    char buffer[FILE_BUFFER_SIZE];
    char current_filename[256] = {0};
    int file_fd = -1;
    size_t total_received = 0;

    while (1)
    {
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&src_addr, &addr_len);
        if (len <= 0) continue;

        buffer[len] = '\0';  // 确保字符串结束

        //处理飞秋客户端发送来的拉取请求：IPMSG_GETFILEDATA:123456
        if (strncmp(buffer, "IPMSG_GETFILEDATA:", 18) == 0) {
            // 解析格式: IPMSG_GETFILEDATA:packno:fino
            char *colon1 = strchr(buffer, ':');     // 第一个 :
            char *colon2 = colon1 ? strchr(colon1 + 1, ':') : NULL;  // 第二个 :
            
            if (colon1 && colon2) {
                char packno[32] = {0};
                char fino[32] = {0};
                strncpy(packno, colon1 + 1, colon2 - colon1 - 1);  // 提取 packno
                strncpy(fino, colon2 + 1, sizeof(fino) - 1);       // 提取 fino

                const char *file_path = find_file_by_id(packno, fino);
                if (file_path) {
                    char ipbuf[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &src_addr.sin_addr, ipbuf, sizeof(ipbuf));
                    printf("[GETFILEDATA] 收到请求：packno=%s fino=%s IP=%s，路径=%s\n",
                        packno, fino, ipbuf, file_path);

                    if (!start_tcp_file_send(ipbuf, 2425, file_path)) {
                        fprintf(stderr, "[GETFILEDATA] TCP 传输失败\n");
                    }
                } else {
                    fprintf(stderr, "[GETFILEDATA] 未找到文件：packno=%s fino=%s\n", packno, fino);
                }
            } else {
                fprintf(stderr, "[GETFILEDATA] 请求格式不正确，无法解析 packno:fino\n");
            }

            continue; // 跳过其他处理
        }



        // ? 文件头：HEADER:filename:size
        if (strncmp(buffer, "HEADER:", 7) == 0)
        {
            if (file_fd > 0) close(file_fd); // 已打开则关闭
            sscanf(buffer, "HEADER:%[^:]:%*s", current_filename);  // 提取文件名
            printf("[recv] 开始接收文件：%s\n", current_filename);
            file_fd = open(current_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            total_received = 0;
            continue;
        }

        // ? 文件结束
        if (strcmp(buffer, "END\n") == 0)
        {
            if (file_fd > 0) close(file_fd);
            printf("[recv] 接收完成：%s，大小约 %zu 字节\n", current_filename, total_received);
            file_fd = -1;
            total_received = 0;
            continue;
        }

        // ? 正常内容
        if (file_fd > 0)
        {
            write(file_fd, buffer, len);
            total_received += len;
        }
    }

    close(sockfd);
    pthread_exit(NULL);
}


//启动这个接收线程1次！
void start_udp_file_receiver(void)
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, udp_file_receiver_thread, NULL) != 0)
    {
        perror("[recv] 创建接收线程失败");
        return;
    }
    pthread_detach(tid);
}
