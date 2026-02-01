#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>

using namespace std;

int setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 你之后会在这里维护：
unordered_map<int, string> recvBuffers;

bool sendAll(int fd, const char *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    size_t n = send(fd, data + sent, len - sent, 0);
    if (n > 0) {
      sent += n;
    } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

void handleClient(int epfd, int client_fd) {
  // 1️⃣ 在这里 recv（注意：非阻塞）
  char buffer[1024];
  while (true) {
    int n = recv(client_fd, buffer, sizeof(buffer), 0);
    // 2️⃣ 判断 n > 0 / n == 0 / n < 0
    if (n > 0) {
      // 3️⃣ 把数据 append 到该 client 的缓存
      recvBuffers[client_fd].append(buffer, n);
      // 4️⃣ 循环拆包（4字节长度 + body）
      while (recvBuffers[client_fd].size() >= 4) {
        uint32_t net_len;
        memcpy(&net_len, recvBuffers[client_fd].data(), 4);
        uint32_t len = ntohl(net_len);
        if (recvBuffers[client_fd].size() >= 4 + len) { //够一个数据包
          // 5️⃣ send 回客户端
          // cout << len << endl;
          // cout.write(recvBuffers[client_fd].data() + 4, len);
          // cout << endl;
          uint32_t send_len = htonl(len);
          sendAll(client_fd, (char *)&send_len, 4);
          if (len > 0) {
            sendAll(client_fd, recvBuffers[client_fd].data() + 4, len);
          }
          // send(client_fd, &send_len, 4, 0);
          // send(client_fd, recvBuffers[client_fd].data() + 4, len, 0);
        } else {
          break;
        }
        //一次性删除一个数据包
        recvBuffers[client_fd].erase(0, 4 + len);
      }

    } else if (n == 0) {
      // 6️⃣ 客户端关闭时：epoll_ctl DEL + close + 清理缓存
      epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, 0);
      close(client_fd);
      return;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        close(client_fd);
        break;
      }
    }
  }
}

int main() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(listen_fd, (sockaddr *)&addr, sizeof(addr));
  listen(listen_fd, 128);

  setNonBlocking(listen_fd);

  int epfd = epoll_create1(0);

  epoll_event ev{};
  ev.events = EPOLLIN; // 之后你可以思考是否要 EPOLLET
  ev.data.fd = listen_fd;
  epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

  epoll_event events[1024];

  while (true) {
    int nready = epoll_wait(epfd, events, 1024, -1);

    for (int i = 0; i < nready; ++i) {
      int fd = events[i].data.fd;

      if (fd == listen_fd) {
        // 1️⃣ accept 新连接
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd > 0) {
          // 2️⃣ setNonBlocking
          setNonBlocking(client_fd);

          // 3️⃣ epoll_ctl ADD 到 epoll
          epoll_event cev{};
          cev.events = EPOLLIN;
          cev.data.fd = client_fd;
          epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
        }

      } else {
        handleClient(epfd, fd);
      }
    }
  }

  close(listen_fd);
  return 0;
}
