//
// Created by victoryang00 on 11/9/23.
//
/** for thread creation and memory monitor */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <logging.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct cxlmemsim_param {
    int sock;
    struct sockaddr_un addr;

} cxlmemsim_param_t;

cxlmemsim_param_t param;
// TODO: add a socket to communicate with the simulator
void call_socket_with_int3() {
    const char *message = "Hello, server!";
    if (sendto(param.sock, message, strlen(message), 0, (struct sockaddr *)&param.addr, sizeof(&param.addr)) < 0) {
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
                                                          void *(*start_routine)(void *), void *arg) {
    LOG(INFO) << "pthread_create";
    return 0;
};

__attribute__((visibility("default"))) int pthread_join(pthread_t thread, void **retval) {
    LOG(INFO) << "pthread_join";
    return 0;
};

__attribute__((visibility("default"))) int pthread_detach(pthread_t thread) {
    LOG(INFO) << "pthread_detach";
    return 0;
};

__attribute__((visibility("default"))) void *mmap(void *addr, size_t length, int prot, int flags, int fd,
                                                  off_t offset) {
    LOG(INFO) << "mmap";
    return nullptr;
};

__attribute__((visibility("default"))) int munmap(void *addr, size_t length) {
    LOG(INFO) << "munmap";
    return 0;
};

__attribute__((visibility("default"))) void *malloc(size_t size) {
    LOG(INFO) << "malloc";
    return 0;
};

__attribute__((visibility("default"))) void free(void *ptr) { LOG(INFO) << "free"; };

__attribute__((constructor)) static void cxlmemsim_constructor() {
    LOG(INFO) << "init";
    param.sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    /** register the original impl */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    // save the original impl of mmap
}

__attribute__((destructor)) void cxlmemsim_destructor() {
    LOG(INFO) << "fini";
    close(param.sock);
}
