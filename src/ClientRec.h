//
// Created by user on 16.10.16.
//

#pragma once

#include <string>
#include <pthread.h>
#include "etcp.h"

using namespace std;

class ClientRec {
private:
    string name = "";
    pthread_t* p_thread;
    volatile bool toClose = false;
    SOCKET localSocketID;
    sockaddr_in* p_remoteSocketAddr;
public:
    ClientRec(pthread_t*, SOCKET, sockaddr_in*);
    //ClientRec(const ClientRec&);
    ClientRec();
    string getName() const;
    pthread_t * getThread() const;
    SOCKET getLocalSocketID() const;
    sockaddr_in* getRemoteSocketAddr() const;
    bool isToClose() const;

    void close();
    void setName(string);
};