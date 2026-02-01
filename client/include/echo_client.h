#ifndef ECHO_CLIENT_H
#define ECHO_CLIENT_H

#include <string>

class EchoClient{
public:
    EchoClient(const std::string& ip, int port);
    ~EchoClient();

    bool connect();
    bool sendMessage(const std::string& msg, std::string& respone);
    void disconnect();

private:
    std::string server_ip_;
    int server_port_;
    int sockfd_;
};

#endif // ECHO_CLIENT_H