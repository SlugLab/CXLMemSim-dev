//
// Created by victoryang00 on 11/9/23.
//
/** for thread creation and memory monitor */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <logging.h>

typedef struct cxlmemsim_param{
    int sock;
    struct sockaddr_un addr;

}cxlmemsim_param_t;

cxlmemsim_param_t param;

void call_socket_with_int3(){
    const char *message = "Hello, server!";
    if (sendto(param.sock, message, strlen(message), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto");
        exit(1);
    }
    strcpy(param.addr.sun_path, SOCKET_PATH);
    remove(param.addr.sun_path);
    if (bind(param.sock, (struct sockaddr *)&param.addr, sizeof(param.addr)) == -1) { // can be blocked for multi thread
        LOG(ERROR) << "Failed to execute. Can't bind to a socket.\n";
        exit(1);
    }
    __asm__("int $0x3");
}

__attribute__((visibility("default"))) int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg){

};

__attribute__((constructor)) static void cxlmemsim_constructor(){
    LOG(INFO) << "init";
    param.sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    /** register the original impl */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path,SOCKET_PATH , sizeof(addr.sun_path) - 1);
}

__attribute__((destructor)) void cxlmemsim_destructor(){
    LOG(INFO) << "fini";
    close(param.sock);
}