#include "etcp.h"

void server(SOCKET s, struct sockaddr_in* peerp) {
    send(s, "Hello, world!", 13, 0);
}

int main(int argc, char** argv) {
    struct sockaddr_in peer;
    char* hostname;
    char* portname;
    socklen_t peerlen;
    SOCKET s1;
    SOCKET s;
    const int on = 1;

    INIT();
    if(argc == 2) {
        hostname = NULL;
        portname = argv[1];
    } else {
        hostname = argv[1];
        portname = argv[2];
    }

    s = tcp_server(hostname, portname);

    do {
        peerlen = sizeof(peer);
        s1 = accept(s, (struct sockaddr*) &peer, &peerlen);
        if(!isvalidsock(s1))
            error(1, errno, "Error in accept() call");
        server(s1, &peer);
        CLOSE(s1);
    } while(1);
    EXIT(0);
}