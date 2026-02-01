#include "utils.h"
#include <unistd.h>
#include <cerrno>
#include <iostream>

int setNonBlocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool sendAll(int fd, const void* data, size_t len) {
    size_t sent = 0;
    const char* p = static_cast<const char*>(data);
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) {
            sent += n;
        } else {
            return false;
        }
    }
    return true;
}

bool recvAll(int fd, void* data, size_t len) {
    size_t recvd = 0;
    char* p = static_cast<char*>(data);
    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n > 0) {
            recvd += n;
        } else {
            return false;
        }
    }
    return true;
}