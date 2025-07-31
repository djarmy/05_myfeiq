#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>

#include "sys_info.h"
#include "msg_handler.h"  // 引入 定义的结构体ReceiverArgs 和线程函数声明 
#include "uiloop.h"
#include "resource_mgr.h"
#include "file_transfer.h"

// gcc  -Wformat code/*.c    -o a     // -I参数指定include目录
//gcc  -Wformat code/*.c    -o a  -lpthread


//程序运行控制标志
volatile bool g_running = true;

// 退出信号处理函数
void handle_exit(int sig) 
{
    printf("\n[系统提示] 接收到退出信号，正在清理资源...\n");
    exit(0);
}


int main(int argc, char const *argv[])
{ 
    //注册退出信号处理函数
    signal(SIGINT, handle_exit);  // Ctrl+C
    signal(SIGTERM, handle_exit); // kill

    //sys_init("rose","rootHost");
    if (sys_init("rose","rootHost") != 0) {
        fprintf(stderr, "初始化接收线程失败，程序退出。\n");
        return EXIT_FAILURE;
    }

    // ✅ 添加这行初始化文件传输功能
    init_file_transfer();

    //初始化接受结构体
    ReceiverArgs recv_args = {0};
    if (prepare_receiver_args(&recv_args) != 0)
    {
        fprintf(stderr, "初始化 ReceiverArgs 失败\n");
        exit(EXIT_FAILURE);
    }

 
    pthread_t r_tid = {0};
    //创建接受线程，处理消息接受和应答逻辑
    if (pthread_create(&r_tid, NULL, recv_msg_thread, &recv_args) != 0)
    {
        fprintf(stderr, "[%s:%d] ",__FILE__, __LINE__);
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
    }

    // 主线程进入 UI 循环
    while (g_running) 
    {
        ui_main_loop();  // 菜单执行一次      // 启动 UI 菜单循环（用户输入）
        sleep(1);        // 防止 UI 高速轮询
    }

    // 通知并等待子线程退出
    printf("[系统提示] 正在等待接收线程退出...\n");
    pthread_cancel(r_tid);      // 发送取消请求
    pthread_join(r_tid, NULL);  // 阻塞等待线程结束

    // 统一资源清理
    cleanup_all_resources();  // 完善的资源释放逻辑
    printf("[系统提示] 程序已正常退出。\n");
    
 
    return 0;
}

 
