#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

using namespace std;

bool sendAll(int fd, const void *data, size_t len) {
  size_t sent = 0;
  const char *p = (const char *)data;

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

bool recvAll(int fd, void *data, size_t len) {
  size_t recvd = 0;
  char *p = (char *)data;
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

int main() {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(8080);
  inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

  connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr));

  string input;

  while (true) {
    cout << ">> ";
    getline(cin, input);
    if (input == "exit")
      break;

    uint32_t len = input.size();
    uint32_t net_len = htonl(len);

    if (!sendAll(sockfd, &net_len, 4))
      break;
    if (len > 0 && !sendAll(sockfd, input.data(), len))
      break;

    uint32_t resp_net_len;
    if (!recvAll(sockfd, &resp_net_len, 4))
      break;

    uint32_t resp_len = ntohl(resp_net_len);

    string resp(resp_len, '\0');
    if (resp_len > 0 && !recvAll(sockfd, resp.data(), resp_len))
      break;

    cout << resp << endl;
  }

  close(sockfd);
  return 0;
}
