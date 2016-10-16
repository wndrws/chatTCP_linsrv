//
// Created by user on 16.10.16.
//

#include "etcp.h"
#include <pthread.h>

void server(SOCKET s, struct sockaddr_in* peerp) {
    send(s, "Hello, world!", 13, 0);
}

SOCKET listening_socket;


pthread_t th_listener;
void* listener_run(void* arg) {
    printf("%s: listener thread started.", program_name);
    printf(" Got arg %d\n", *(int*)arg);
    sockaddr_in peer;
    socklen_t peerlen;
    do {
        SOCKET s1;
        peerlen = sizeof(peer);
        s1 = accept(listening_socket, (struct sockaddr*) &peer, &peerlen);
        if(!isvalidsock(s1))
            error(1, errno, "Error in accept() call");
        server(s1, &peer);
        CLOSE(s1);
    } while(1);
}

int main(int argc, char** argv) {
    char* hostname;
    char* portname;
    char str[256];

    INIT();
    if(argc == 2) {
        hostname = NULL;
        portname = argv[1];
    } else {
        hostname = argv[1];
        portname = argv[2];
    }

    listening_socket = tcp_server(hostname, portname);
    int arg = 6;
    pthread_create(&th_listener, NULL, listener_run, (void*) &arg);
    for( ; ; ) {
        printf("Say something, please\n");
        scanf("%s", str);
        if(strcmp(str, "q") == 0) break;
        printf("You've said: %s\n", str);
    }
    EXIT(0);
}