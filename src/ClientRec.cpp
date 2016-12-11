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

SOCKET ClientRec::getSocketID() const {
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
    int r = readvrec(getSocketID(), username, MAX_USERNAME_LENGTH);
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
    r = send(getSocketID(), msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to send users list to " << getFullName() << endl;
    }
    notify(NotificationType::LOGIN);
}

string ClientRec::formUsersList() const {
    ostringstream ss;
    for(auto it = clients.cbegin(); it != clients.cend(); ++it) {
        ss << (uint32_t) it->first << '\n' << it->second.getName() << '\n';
    }
    ss << "\4\n"; //End Of Transmission
    return ss.str();
}

void ClientRec::notifyIn(int id, const string& name) {
    m_notified = true;
    m_loggedIn.push_back(to_string(id));
    m_loggedIn.push_back(name);
}

void ClientRec::notifyOut(int id) {
    m_notified = true;
    m_loggedOut.push_back(to_string(id));
}

inline void ClientRec::notify(NotificationType nt) const {
    for (auto it = clients.cbegin(); it != clients.cend(); ++it) {
        if (it->first != m_id) { // Without this condition function won't be const
            switch (nt) {
                case LOGIN: it->second.notifyIn(m_id, m_name); break;
                case LOGOUT: it->second.notifyOut(m_id); break;
                default: cerr << "Illegal argument for notify()" << endl; return;
            }
        }
    }
}

void ClientRec::sendNotifications() {
    if(!m_notified) return;
    if(m_loggedIn.empty() && m_loggedOut.empty()) {
        cerr << "Error: notified by nobody." << endl;
        return;
    }

    ostringstream ss;
    string msg;
    int r;

    if(!m_loggedIn.empty()) {
        for (int i = 0; i < m_loggedIn.size(); i += 2) {
            ss << m_loggedIn[i] << '\n' << m_loggedIn[i + 1] << '\n';
        }
        ss << "\4\n"; //End Of Transmission
        msg = ss.str();
        msg.insert(0, 1, (char) CODE_LOGINNOTIFY);
        r = send(getSocketID(), msg.c_str(), msg.size(), 0);
        if (r == -1) {
            cerr << "Failed to send login notification to " << getFullName() << endl;
        } else m_loggedIn.clear();

        ss.str("");
    }

    if(!m_loggedOut.empty()) {
        for (int i = 0; i < m_loggedOut.size(); i++) {
            ss << m_loggedOut[i] << '\n';
        }
        ss << "\4\n"; //End Of Transmission
        msg = ss.str();
        msg.insert(0, 1, (char) CODE_LOGOUTNOTIFY);
        r = send(getSocketID(), msg.c_str(), msg.size(), 0);
        if (r == -1) {
            cerr << "Failed to send logout notification to " << getFullName() << endl;
        } else m_loggedOut.clear();
    }
    m_notified = false;
}

void ClientRec::logout() const {
    char code = CODE_LOGOUTANSWER;
    int r = send(getSocketID(), &code, 1, 0);
    if(r == -1) {
        cerr << "Failed to send logout answer to " << getFullName() << endl;
    }
    notify(NotificationType::LOGOUT);
}

void ClientRec::forcedLogout() const {
    char code = CODE_FORCEDLOGOUT;
    int r = send(getSocketID(), &code, 1, 0);
    if(r == -1) {
        cerr << "Failed to send forced logout message to " << getFullName();
    }
    notify(NotificationType::LOGOUT);
    cout << "User " << getFullName() << " was logged out by force." << endl;
}

void ClientRec::sendErrorMsg(int errcode, const string& descr) const {
    string msg = "Server error " + to_string(errcode) + ":\n" + descr + "\n";
    uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    msg.insert(0, 1, (char) CODE_SRVERR);
    msg.insert(1, (char*) &len, 2);
    int r = send(getSocketID(), msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to send error message to " << getFullName() << endl;
    }
}

void ClientRec::sendMsg(const string &text) const {
    string msg = text + "\n";
    uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    msg.insert(0, 1, (char) CODE_SRVMSG);
    msg.insert(1, (char*) &len, 2);
    int r = send(getSocketID(), msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to send message to " << getFullName() << endl;
    }
}

bool ClientRec::transmitMsg() const {
    char buf [MAX_MSG_LENGTH+1];
    char id_buf [11]; //max 10 digits in a 32-bit id + '\n'
    int id;

    int r = readline(getSocketID(), id_buf, sizeof(id_buf));
    if(r < 0) {
        cerr << "Failed to extract user id from incoming message from "
             << getFullName() << endl;
    }
    id = atoi(id_buf);
    r = readline(getSocketID(), buf, sizeof(buf));
    if(r <= 0) {
        cerr << "Failed to read incoming message from " << getFullName() << endl;
        return false;
    }

    if(clients.find(id) == clients.cend()) return false;

    string msg(buf);
    //uint16_t len = (uint16_t) htons((uint16_t) msg.size());
    //msg.insert(0, (char*) &len, 2);
    msg = to_string(getSocketID()) + "\n" + msg; // Already contains trailing "\n"
    msg.insert(0, 1, (char) CODE_OUTMSG);
    r = send(clients.at(id).getSocketID(), msg.c_str(), msg.size(), 0);
    if(r == -1) {
        cerr << "Failed to transmit message to " << clients.at(id).getFullName() << endl;
        return false;
    }
    return true;
}