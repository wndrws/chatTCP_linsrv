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
    unsigned char code = 255;
    int rcvdb, id = 0;
    string name = "<unknown>";
    tbb::concurrent_unordered_map<SOCKET, ClientRec>::const_iterator it;
    int mode = 0;

    while(!clients.at(s).isToClose()) {
        if(mode == 0) {
            mode = 1;
            fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); // Make socket non-blocking
        }
        rcvdb = readn(s, (char*) &code, 1);
        if(rcvdb == 0) {
            clients.at(s).notify(NotificationType::LOGOUT);
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
                clients.at(s).notify(NotificationType::LOGOUT);
                clients.at(s).close();
            }
        } else {
            mode = 0;
            fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) & !O_NONBLOCK); // Make socket blocking again
            switch (code) {
                case CODE_LOGINREQUEST:
                    clients.at(s).login();
                    id = clients.at(s).getSocketID();
                    name = clients.at(s).getName();
                    cout << "User " + name+"#"+to_string(id) + " logged in!" << endl;
                    break;
                case CODE_LOGOUTREQUEST:
                    clients.at(s).logout();
                    cout << "User " + name+"#"+to_string(id) + " logged out!" << endl;
                    clients.at(s).close();
                    break;
                case CODE_INMSG:
                    if(!clients.at(s).transmitMsg()) {
                        clients.at(s).sendErrorMsg(42, "Failed to transmit the message");
                    }
                    break;
                default:
                    // Send heartbeat
                    int r = send(s, "Hello, world!\n", 13, 0);
                    if (r < 0) {
                        if (!clients.at(s).getName().empty())
                            cout << "User " + name+"#"+to_string(id) + " is gone." << endl;
                        clients.at(s).notify(NotificationType::LOGOUT);
                        clients.at(s).close();
                    }
                    sleep(1);
                    break;
            }
        }
        code = 255;
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
        // Accept clients to blocking sockets
        s = accept(listening_socket, (struct sockaddr*) &peer, &peerlen);
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
        getline(cin, str);
        if(str == "q") break;
        else if(str.length() > 2) {
            if (str.at(0) == 'b') {
                string userToBan = str.substr(2);
                int id = findClient(userToBan);
                if (id != -1) {
                    clients.at(id).forcedLogout();
                    clients.at(id).close();
                }
            } else if (str.at(0) == 'm') {
                string userToInform = str.substr(2);
                int id = findClient(userToInform);
                if (id != -1) {
                    string msg;
                    cout << "Type message for " << userToInform << ":" << endl;
                    getline(cin, msg);
                    clients.at(id).sendMsg(msg);
                }
            }
        }
        else cout << "You've said: " << str << endl;
    }
    //Traverse clients calling close() on each one and joining their threads.
    for(auto&& client : clients) {
        client.second.forcedLogout();
        cout << "Closing socket " << client.first << endl;
        client.second.close();
    }
    for(auto&& client : clients) {
        cout << "Joining thread " << *(client.second.getThread()) << endl;
        int retval = 0;
        int err = pthread_join(*(client.second.getThread()), (void**) &retval);
        if(err != 0) {
            cerr << "Cannot join. Error: " << strerror(err) << endl;
        }
        cout << "Joined with exit status " << retval << endl;
        delete client.second.getThread();
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