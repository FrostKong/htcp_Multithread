#pragma once
#include "noncopyable.h"
class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    //防止隐式产生临时对象
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
    }

    ~Socket();

    int fd() const { return sockfd_;}
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress * peeraddr);

    void shutdownWrite();//用不上

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
    const int sockfd_;
};