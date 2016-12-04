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

int findClient(string IDorName);

void* connectionToClient(void* sock) {
    SOCKET s = *((SOCKET*) sock);
    char code = -1;
    int rcvdb, id = 0;
    string name = "<unknown>";
    tbb::concurrent_unordered_map<SOCKET, ClientRec>::const_iterator it;

    while(!clients.at(s).isToClose()) {
        rcvdb = readn(s, &code, 1);
        if(rcvdb == 0) {
            for (it = clients.cbegin(); it != clients.cend(); ++it) {
                if (it->first != s) it->second.notifyOut(id);
            }
            clients.at(s).close();
            cout << "User " + name+"#"+to_string(id) + " disconnected gracefully." << endl;
        } else if(rcvdb < 0) {
            int error = errno;
            if(error == EWOULDBLOCK || error == EAGAIN || error == EINTR) {
                // It's ok, continue doing job after some time
                usleep(200000); // sleep for 0.2 seconds
            } else {
                cerr << "Error: reading from socket " << s << endl;
                cerr << "Disconnecting..." << endl;
                for (it = clients.cbegin(); it != clients.cend(); ++it) {
                    if (it->first != s) it->second.notifyOut(id);
                }
                clients.at(s).close();
            }
        } else {
            switch (code) {
                case CODE_LOGINREQUEST:
                    clients.at(s).login();
                    id = clients.at(s).getLocalSocketID();
                    name = clients.at(s).getName();
                    cout << "User " + name+"#"+to_string(id) + " logged in!" << endl;
                    for (it = clients.cbegin(); it != clients.cend(); ++it) {
                        if (it->first != s) it->second.notifyIn(id, name);
                    }
                    break;
                case CODE_LOGOUTREQUEST:
                    clients.at(s).logout();
                    cout << "User " + name+"#"+to_string(id) + " logged out!" << endl;
                    for (it = clients.cbegin(); it != clients.cend(); ++it) {
                        if (it->first != s) it->second.notifyOut(id);
                    }
                    clients.at(s).close();
                    break;
                default:
                    // Send heartbeat
                    int r = send(s, "Hello, world!\n", 13, 0);
                    if (r < 0) {
                        if (!clients.at(s).getName().empty())
                            cout << "User " + name+"#"+to_string(id) + " is gone." << endl;
                        for (it = clients.cbegin(); it != clients.cend(); ++it) {
                            if (it->first != s) it->second.notifyOut(id);
                        }
                        clients.at(s).close();
                    }
                    sleep(1);
                    break;
            }
        }
        code = -1;
        clients.at(s).sendNotifications();
    }
    shutdown(s, SHUT_RDWR);
    cerr << "Info: Socket " << s << " is shut down." << endl;
    CLOSE(s);
    cerr << "Info: Socket " << s << " is closed." << endl;
    {
        SCOPE_LOCK_MUTEX(mutex.get());
        clients.unsafe_erase(s);
    }
    cerr << "Info: Client at socket " << s << " is erased from users list." << endl;
    pthread_exit(0);
}

pthread_t th_listener;
void* listener_run(void*) {
    //printf("%s: listener thread started.\n", program_name);
    SOCKET s;
    sockaddr_in peer;
    socklen_t peerlen = sizeof(peer);

    for( ; ; ) {
        // Accept clients to non-blocking sockets
        s = accept4(listening_socket, (struct sockaddr*) &peer, &peerlen, SOCK_NONBLOCK);
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
        else if(str == "b") {
            string userToBan;
            cin >> userToBan;
            int id = findClient(userToBan);
            if(id != -1) {
                clients.at(id).forcedLogout();
                clients.at(id).close();
            }
        } else if(str == "m") {
            string userToInform;
            cin >> userToInform;
            int id = findClient(userToInform);
            if(id != -1) {
                string msg;
                cout << "Type message for " << userToInform << ":" << endl;
                cin >> msg;
                clients.at(id).sendMsg(msg);
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
            cerr << "Cannot join. Error: " << strerror(err) << endl;
        }
        cout << "Joined with exit status " << retval << endl;
        delete it->second.getThread();
    }
    EXIT(0);
}

int findClient(string IDorName) {
    unsigned int pos = IDorName.find('#');
    if(pos != string::npos) {
        // This is ID
        int id = atoi(IDorName.substr(pos+1).c_str());
        auto it = clients.find(id);
        if(it != clients.cend()) {
            return it->first;
        } else {
            cout << "User with id " << id << " not found." << endl;
        }
    } else {
        auto it = clients.cbegin();
        for( ; it != clients.cend(); ++it) {
            if(it->second.getName() == IDorName) {
                return it->first;
            }
        }
        if(it == clients.cend()) {
            cout << "User \"" + IDorName + "\" not found." << endl;
        }
    }
    return -1;
}