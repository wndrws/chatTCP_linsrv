//
// Created by user on 16.10.16.
//

#ifdef __cplusplus
extern "C" {
#endif
char* program_name;
#ifdef __cplusplus
}
#endif

#include "etcp.h"
#include <pthread.h>
#include <iostream>
#include "tbb/concurrent_unordered_map.h"
#include "ClientRec.h"

SOCKET listening_socket;
bool stop = false;
tbb::concurrent_unordered_map<SOCKET, ClientRec> clients;

void* connectionToClient(void* sock) {
    SOCKET s = *((SOCKET*) sock);
    while(!clients.at(s).isToClose()) {
        send(s, "Hello, world!\n", 13, 0);
        sleep(1);
    }
    CLOSE(s);
    pthread_exit(0);
}

pthread_t th_listener;
void* listener_run(void*) {
    printf("%s: listener thread started.", program_name);
    SOCKET s;
    sockaddr_in peer;
    socklen_t peerlen = sizeof(peer);

    for( ; ; ) {
        s = accept(listening_socket, (sockaddr*) &peer, &peerlen);
        if(!isvalidsock(s))
            error(1, errno, "Error in accept() call");
        if(stop) {
            CLOSE(s);
            break;
        }
        pthread_t* pth = new pthread_t;
        ClientRec newClient(pth, s, &peer);
        clients[s] = newClient;
        pthread_create(pth, NULL, connectionToClient, (void*) &s);
    }
    CLOSE(listening_socket);
    pthread_exit(0);
}

int main(int argc, char** argv) {
    char* hostname;
    char* portname;

    INIT();
    if(argc == 2) {
        hostname = NULL;
        portname = argv[1];
    } else {
        hostname = argv[1];
        portname = argv[2];
    }

    listening_socket = tcp_server(hostname, portname);
    pthread_create(&th_listener, NULL, listener_run, NULL);
    for( ; ; ) {
        string str;
        cout << "Say something, please\n";
        cin >> str;
        if(str == "q") break;
        cout << "You've said: " << str << endl;
    }
    //Traverse clients calling close() on each one and joining their threads.
    for(tbb::concurrent_unordered_map<SOCKET, ClientRec>::iterator it = clients.begin();
            it != clients.end(); ++it) {
        cout << "Closing socket " << it->first << endl;
        it->second.close();
    }
    for(tbb::concurrent_unordered_map<SOCKET, ClientRec>::iterator it = clients.begin();
        it != clients.end(); ++it) {
        cout << "Joining thread " << *(it->second.getThread()) << endl;
        int retval = 0;
        int err = pthread_join(*(it->second.getThread()), (void**) &retval);
        if(err != 0) {
            cout << "Cannot join. Error: " << strerror(err) << endl;
        }
        cout << "Joined with exit status " << retval << endl;
        delete it->second.getThread();
    }
    EXIT(0);
}