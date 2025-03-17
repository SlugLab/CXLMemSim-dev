#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sched.h>
// set_tid_address 系统调用号
#define SYS_set_tid_address 218

// 用于存储线程ID的变量
int *clear_child_tid = NULL;

// 线程函数
int thread_function(void *arg) {
    printf("子线程: 我的 TID 是 %ld\n", syscall(SYS_gettid));
    printf("子线程: clear_child_tid 地址是 %p\n", clear_child_tid);
    
    // 让线程运行一段时间
    sleep(2);
    
    printf("子线程: 退出前 clear_child_tid 的值是 %d\n", *clear_child_tid);
    return 0;
}

int main() {
    // 分配内存用于存储线程ID
    clear_child_tid = (int *)malloc(sizeof(int));
    *clear_child_tid = 0;
    
    printf("主线程: clear_child_tid 地址是 %p\n", clear_child_tid);
    
    // 分配线程栈
    void *stack = malloc(4096);
    if (!stack) {
        perror("malloc 失败");
        return 1;
    }
    
    // 创建子线程
    pid_t child_pid = clone(thread_function, 
                           (char *)stack + 4096,  // 栈顶
                           CLONE_VM | CLONE_FS | CLONE_FILES | SIGCHLD, 
                           NULL);
    
    if (child_pid == -1) {
        perror("clone 失败");
        free(stack);
        free(clear_child_tid);
        return 1;
    }
    
    printf("主线程: 创建的子线程 ID 是 %d\n", child_pid);
    
    // 直接调用 set_tid_address 系统调用
    long result = syscall(SYS_set_tid_address, clear_child_tid);
    printf("主线程: set_tid_address 返回值是 %ld\n", result);
    
    // 等待子线程结束
    waitpid(child_pid, NULL, 0);
    
    printf("主线程: 子线程结束后 clear_child_tid 的值是 %d\n", *clear_child_tid);
    
    // 释放资源
    free(stack);
    free(clear_child_tid);
    
    return 0;
}
