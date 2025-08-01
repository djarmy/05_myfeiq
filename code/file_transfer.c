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

#include "file_transfer.h"
#include "file_transfer.h"     // 引入头文件
#include "msg_handler.h"
#include "user_list.h"
#include "file_transfer_tcp.h"  // ✅ 引入 TCP 文件发送头文件

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
        perror("[file_transfer] open failed");
        pthread_exit(NULL);             //第一个参数就是线程执行者 退出     。退出当前正在执行的线程。精准终止 file_send_thread 线程，无需额外指定(当 file_send_thread_func 函数被新线程执行时就关联了上下文)。
    }
    
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

    while (sent_size < total_size)             //这正是 “分块读取 + 发送” 的设计目的
    {
        //读取一块数据
        ssize_t read_bytes = read(fd, buffer, FILE_CHUNK_SIZE);    
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
    }
    fprintf(stderr, "[%s:%d]文件发送完成，累计已发送总字节数：%zu\n", __FILE__, __LINE__, sent_size);
    close(fd);                                         // 关闭文件
    pthread_exit(NULL);                                // 正常结束线程
}



// 从用户编号和文件路径，发送文件
void send_file_to_user(int user_id, const char *filepath)
{
    if (!filepath || strlen(filepath) == 0) {
        printf("[错误] 文件路径为空，发送失败！\n");
        return;
    }

    const UserInfo *info = get_user_info(user_id);  // 获取用户信息
    if (!info) {
        printf("[错误] 用户编号无效，发送失败！\n");
        return;
    }
 
    
    // ✅ 设定 TCP 文件传输端口（客户端连接接收方）
    const uint16_t FILE_TCP_PORT = 9527;

    printf("[提示] 正在通过 TCP 发送文件 \"%s\" 给 %s@%s [%s] ...\n",
           filepath, info->username, info->hostname, info->ipaddr);

    // ✅ 使用 TCP 实现文件发送（新逻辑）
    if (!start_tcp_file_send(info->ipaddr, FILE_TCP_PORT, filepath)) {
        printf("[错误] TCP 文件发送失败！\n");
    } else {
        printf("[成功] 文件发送已启动，后台线程中传输...\n");
    }
}