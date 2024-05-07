#pragma once

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1"); // 不用回环loopback
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {
    }
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t port() const; // const有什么讲究吗？ uint16_t有什么讲究吗？ lookat 这里是toPort

    const sockaddr_in *getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};