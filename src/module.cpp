//
// Created by victoryang00 on 11/9/23.
//

#define SOCKET_PATH "/tmp/cxl_mem_simulator.sock"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <logging.h>

void call_socket_with_int3(){
    const char *message = "Hello, server!";
    if (sendto(sock, message, strlen(message), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto");
        exit(1);
    }
    strcpy(addr.sun_path, SOCKET_PATH);
    remove(addr.sun_path);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) { // can be blocked for multi thread
        LOG(ERROR) << "Failed to execute. Can't bind to a socket.\n";
        exit(1);
    }

}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);


int main(int argc, char *argv[]) {
}