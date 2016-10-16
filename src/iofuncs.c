//
// Created by user on 16.10.16.
//

#include "etcp.h"
#define RECLEN_TYPE uint16_t

// Прочесть len байтов из сокета sock и записать по указателю bp.
int readn(SOCKET sock, char* bp, size_t len) {
    int cnt, rc;

    cnt = len;
    while(cnt > 0) {
        rc = recv(sock, bp, cnt, 0);
        if(rc < 0) {                // Ошибка чтения?..
            if(errno == EINTR)      // Или вызов был прерван?
                continue;           // Повторить чтение
            return -1;              // Вернуть код ошибки
        }
        if(rc == 0) {               // Если ничего не принято,
            return len - cnt;       // вернуть неполный счетчик.
        }
        bp += rc;
        cnt -= rc;
    }
    return len;
}

// Прочесть из сокета sock запись переменной длины и записать по
// по указателю bp. Максимальный размер буфера для записи равен len.
// Максимальный размер записи определяется типом RECLEN_TYPE.
int readvrec(SOCKET sock, char* bp, size_t len) {
    RECLEN_TYPE reclen;
    int rc;

    // Прочитать длину записи:
    rc = readn(sock, (char*) &reclen, sizeof(RECLEN_TYPE));
    // Проверка, что считалось верное число байт:
    if(rc != sizeof(RECLEN_TYPE)) return rc < 0 ? -1 : 0;
    switch (sizeof(RECLEN_TYPE)) {
        case 2: reclen = ntohs(reclen); //uint16_t
        case 4: reclen = ntohl(reclen); //uint32_t
        default: break;
    }
    if(reclen > len) {
        // В буфере не хватает места для размещения данных -
        // сохраняем, что влезает, а остальное отбрасываем.
        while(reclen > 0) {
            rc = readn(sock, bp, len);
            if(rc != len) return rc < 0 ? -1 : 0;
            reclen -= len;
            if(reclen < len) len = reclen;
        }
        set_errno(EMSGSIZE);
        return -1;
    }
    // Если всё в порядке, читаем запись:
    rc = readn(sock, bp, reclen);
    if(rc != reclen) return rc < 0 ? -1 : 0;
    return rc;
}