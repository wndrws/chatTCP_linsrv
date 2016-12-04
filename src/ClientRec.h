//
// Created by user on 16.10.16.
//

#pragma once

#include <string>
#include <pthread.h>
#include "etcp.h"

#define CODE_SRVMSG 0
#define CODE_LOGINREQUEST 1
#define CODE_LOGOUTREQUEST 2
#define CODE_LOGINANSWER 3
#define CODE_LOGOUTANSWER 4
#define CODE_FORCEDLOGOUT 5
#define CODE_LOGINNOTIFY 6
#define CODE_LOGOUTNOTIFY 7
#define CODE_SRVERR 8
#define CODE_HEARTBEAT 9

#define CODE_INMSG 128
#define CODE_OUTMSG 129

#define MAX_USERNAME_LENGTH 32

using namespace std;

enum NotificationType { LOGIN, LOGOUT };

class ClientRec {
private:
    string m_name = "";
    pthread_t* p_thread;
    volatile bool toClose = false;
    SOCKET m_id;
    sockaddr_in* p_sockaddr_in;

    volatile bool m_notified = false;
    vector<string> m_loggedIn;
    vector<string> m_loggedOut;

    string formUsersList() const;
public:
    ClientRec(pthread_t*, SOCKET, sockaddr_in*);
    //ClientRec(const ClientRec&);
    ClientRec();
    string getName() const;
    string getFullName() const;
    pthread_t * getThread() const;
    SOCKET getLocalSocketID() const;
    sockaddr_in* getSockaddr_in() const;
    bool isToClose() const;

    inline void notify(NotificationType) const;

    void close();
    void setName(const string&);
    void login();
    void logout() const;
    void forcedLogout() const;
    void notifyIn(int id, const string& username);
    void notifyOut(int id);
    void sendNotifications();
    void sendErrorMsg(int errcode, const string& descr) const;
    void sendMsg(const string& text) const;
};