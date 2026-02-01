#ifndef UTILS_H
#define UTILS_H

#include <sys/socket.h>
#include <cstddef>
#include <fcntl.h>

int setNonBlocking(int fd);
bool sendAll(int fd, const void* data, size_t len);
bool recvAll(int fd, void* data, size_t len);

#endif // UTILS_H