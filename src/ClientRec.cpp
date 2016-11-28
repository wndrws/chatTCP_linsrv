//
// Created by user on 16.10.16.
//

#include "ClientRec.h"

ClientRec::ClientRec(pthread_t* p_thread, SOCKET s, sockaddr_in* addr) {
    this->p_thread = p_thread;
    localSocketID = s;
    p_remoteSocketAddr = addr;
}

//ClientRec::ClientRec(const ClientRec &obj) {
//    name = obj.getName();
//    p_thread = obj.getThread();
//    localSocketID = obj.getLocalSocketID();
//    p_remoteSocketAddr = obj.getRemoteSocketAddr();
//}

ClientRec::ClientRec() {
    p_thread = NULL;
    p_remoteSocketAddr = NULL;
    localSocketID = 0;
}

void ClientRec::close() {
    toClose = true;
}

void ClientRec::setName(string name) {
    this->name = name;
}

string ClientRec::getName() const {
    return name;
}

pthread_t * ClientRec::getThread() const{
    return p_thread;
}

SOCKET ClientRec::getLocalSocketID() const {
    return localSocketID;
}

sockaddr_in* ClientRec::getRemoteSocketAddr() const {
    return p_remoteSocketAddr;
}

bool ClientRec::isToClose() const {
    return toClose;
}

void ClientRec::login() {
    char username [MAX_USERNAME_LENGTH];
    int r = readvrec(localSocketID, username, MAX_USERNAME_LENGTH);
    if(r == -1) {
        printf("Not all bytes are received!");
    }
    setName(string(username));
    // Temporarily
    string msg = "You are successfully logged in!\n";
    uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    msg.insert(0, 1, (char) CODE_SRVMSG);
    msg.insert(1, (char*) &len, 2);
    r = send(localSocketID, msg.c_str(), msg.size(), 0);
    if(r == -1) {
        printf("Failed to send message to user!\n");
    }
}