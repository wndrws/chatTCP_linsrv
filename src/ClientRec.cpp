//
// Created by user on 16.10.16.
//

#include <tbb/concurrent_unordered_map.h>
#include <sstream>
#include <iostream>
#include "ClientRec.h"

extern tbb::concurrent_unordered_map<SOCKET, ClientRec> clients;

ClientRec::ClientRec(pthread_t* p_thread, SOCKET s, sockaddr_in* addr) {
    this->p_thread = p_thread;
    m_id = s;
    p_sockaddr_in = addr;
}

//ClientRec::ClientRec(const ClientRec &obj) {
//    m_name = obj.getName();
//    p_thread = obj.getThread();
//    m_id = obj.getLocalSocketID();
//    p_sockaddr_in = obj.getSockaddr_in();
//}

ClientRec::ClientRec() {
    p_thread = NULL;
    p_sockaddr_in = NULL;
    m_id = 0;
}

void ClientRec::close() {
    toClose = true;
}

void ClientRec::setName(const string& name) {
    m_name = name;
}

string ClientRec::getName() const {
    return m_name;
}

string ClientRec::getFullName() const {
    return m_name + "#" + to_string(m_id);
}

pthread_t * ClientRec::getThread() const{
    return p_thread;
}

SOCKET ClientRec::getLocalSocketID() const {
    return m_id;
}

sockaddr_in* ClientRec::getSockaddr_in() const {
    return p_sockaddr_in;
}

bool ClientRec::isToClose() const {
    return toClose;
}

void ClientRec::login() {
    char username [MAX_USERNAME_LENGTH+1];
    int r = readvrec(m_id, username, MAX_USERNAME_LENGTH);
    if(r == -1) {
        cerr << "Error while reading request for login." << endl;
        return;
    }
    username[r] = '\0';
    setName(string(username));
    //Send users list
    string msg = formUsersList();
    //uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    msg.insert(0, 1, (char) CODE_LOGINANSWER);
    //msg.insert(1, (char*) &len, 2);
    r = send(m_id, msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to send users list to " << getFullName() << endl;
    }
}

string ClientRec::formUsersList() const {
    ostringstream ss;
    for(auto&& it = clients.cbegin(); it != clients.cend(); ++it) {
        ss << (uint32_t) it->first << '\n' << it->second.getName() << '\n';
    }
    ss << "\4\n"; //End Of Transmission
    return ss.str();
}

void ClientRec::notifyIn(int id, const string& name) {
    notified = true;
    loggedIn.push_back(to_string(id));
    loggedIn.push_back(name);
}

void ClientRec::notifyOut(int id) {
    notified = true;
    loggedOut.push_back(to_string(id));
}

void ClientRec::sendNotifications() {
    if(!notified) return;
    if(loggedIn.empty() && loggedOut.empty()) {
        cerr << "Error: notified by nobody." << endl;
        return;
    }

    ostringstream ss;
    string msg;
    int r;

    if(!loggedIn.empty()) {
        for (int i = 0; i < loggedIn.size(); i += 2) {
            ss << loggedIn[i] << '\n' << loggedIn[i + 1] << '\n';
        }
        msg = ss.str();
        msg.insert(0, 1, (char) CODE_LOGINNOTIFY);
        r = send(m_id, msg.c_str(), msg.size(), 0);
        if (r == -1) {
            cerr << "Failed to send login notification to " << getFullName() << endl;
        } else loggedIn.clear();

        ss.str("");
    }

    if(!loggedOut.empty()) {
        for (int i = 0; i < loggedOut.size(); i++) {
            ss << loggedOut[i] << '\n';
        }
        msg = ss.str();
        msg.insert(0, 1, (char) CODE_LOGOUTNOTIFY);
        r = send(m_id, msg.c_str(), msg.size(), 0);
        if (r == -1) {
            cerr << "Failed to send logout notification to " << getFullName() << endl;
        } else loggedOut.clear();
    }
    notified = false;
}

void ClientRec::logout() const {
    char code = CODE_LOGOUTANSWER;
    int r = send(m_id, &code, 1, 0);
    if(r == -1) {
        cerr << "Failed to send logout answer to " << getFullName() << endl;
    }
}

void ClientRec::forcedLogout() const {
    char code = CODE_FORCEDLOGOUT;
    int r = send(m_id, &code, 1, 0);
    if(r == -1) {
        cerr << "Failed to send forced logout message to " << getFullName();
    }
    cout << "User " << getFullName() << " was logged out by force." << endl;
}

void ClientRec::sendErrorMsg(int errcode, const string& descr) const {
    string msg = "Server error " + to_string(errcode) + ":\n" + descr + "\n";
    uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    msg.insert(0, 1, (char) CODE_SRVERR);
    msg.insert(1, (char*) &len, 2);
    int r = send(m_id, msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to send error message to " << getFullName() << endl;
    }
}

void ClientRec::sendMsg(const string &text) const {
    string msg = text + "\n";
    uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    msg.insert(0, 1, (char) CODE_SRVMSG);
    msg.insert(1, (char*) &len, 2);
    int r = send(m_id, msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to send message to " << getFullName() << endl;
    }
}