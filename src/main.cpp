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
#include "automutex.h"

SOCKET listening_socket;
bool stop = false;
tbb::concurrent_unordered_map<SOCKET, ClientRec> clients;
static CAutoMutex mutex;

void* connectionToClient(void* sock) {
    SOCKET s = *((SOCKET*) sock);
    char code = -1;
    int id = 0;
    string name = "<unknown>";
    tbb::concurrent_unordered_map<SOCKET, ClientRec>::const_iterator it;

    while(!clients.at(s).isToClose()) {
        readn(s, &code, 1);
        switch(code) {
            case CODE_LOGINREQUEST:
                clients.at(s).login();
                id = clients.at(s).getLocalSocketID();
                name = clients.at(s).getName();
                cout << "User " + name + "#" + to_string(id) + " logged in!" << endl;
                for(it = clients.cbegin(); it != clients.cend(); ++it) {
                    if(it->first != s) it->second.notifyIn(id, name);
                }
                break;
            case CODE_LOGOUTREQUEST:
                clients.at(s).logout();
                cout << "User " + name + "#" + to_string(id) + " logged out!" << endl;
                for(it = clients.cbegin(); it != clients.cend(); ++it) {
                    if(it->first != s) it->second.notifyOut(id);
                }
                clients.at(s).close();
                break;
            default:
                // Send heartbeat
                int r = send(s, "Hello, world!\n", 13, 0);
                if(r < 0) {
                    if(!clients.at(s).getName().empty())
                        cout << "User " + name + "#" + to_string(id) + " is gone." << endl;
                    for(it = clients.cbegin(); it != clients.cend(); ++it) {
                        if(it->first != s) it->second.notifyOut(id);
                    }
                    clients.at(s).close();
                }
                sleep(1);
                break;
        }
        code = -1;
        clients.at(s).sendNotifications();
    }
    shutdown(s, SHUT_RDWR);
    CLOSE(s);
    {
        SCOPE_LOCK_MUTEX(mutex.get());
        clients.unsafe_erase(s);
    }
    pthread_exit(0);
}

pthread_t th_listener;
void* listener_run(void*) {
    printf("%s: listener thread started.\n", program_name);
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
        if(str.at(0) == 'b') {
            string userToBan = str.substr(2);
            unsigned int pos = userToBan.find('#');
            if(pos != string::npos) {
                // This is ID
                int id = atoi(userToBan.substr(pos+1));
                auto it = clients.find(id);
                if(it != clients.cend()) {
                    it->second.forcedLogout();
                    it->second.close();
                } else {
                    cout << "User with id " << id << " not found." << endl;
                }
            } else {
                auto it = clients.cbegin();
                for( ; it != clients.cend(); ++it) {
                    if(it->second.getName() == userToBan) {
                        it->second.forcedLogout();
                        it->second.close();
                        break;
                    }
                }
                if(it == clients.cend()) {
                    cout << "User \"" + userToBan + "\" not found." << endl;
                }
            }
        }
        else cout << "You've said: " << str << endl;
    }
    //Traverse clients calling close() on each one and joining their threads.
    for(auto&& it = clients.begin(); it != clients.end(); ++it) {
        cout << "Closing socket " << it->first << endl;
        it->second.close();
    }
    for(auto&& it = clients.begin(); it != clients.end(); ++it) {
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