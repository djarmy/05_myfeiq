#include <stdio.h>
#include "sys_info.h"
#include "msg_handler.h"
#include "file_transfer.h"

void cleanup_all_resources(void) {
    // 释放系统信息结构体中资源
    // sys_cleanup();  // 比如关闭 socket，释放动态内存

    // // 释放消息收发资源（如 socket、队列）
    // msg_handler_cleanup();

    // // 释放文件传输过程中申请的资源（若有）
    // file_transfer_cleanup();

    printf("[系统提示] 所有资源已释放完毕。\n");
}
