#include "InetAddress.h"
#include <strings.h>
#include <string.h>
#include <stdio.h>
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_); // 这样子的形式可以吗
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);                  // 主机转网络字节序；port就是这种
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // 十进制ip转成长整形
}

std::string InetAddress::toIp() const
{
    // addr_  应该是拿到ip
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); //::顶层命名空间，相当于全局变量；点分十进制转为二进制整数
    return buf;
}

std::string InetAddress::toIpPort() const
{
    // ip:port  应该是拿到ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // 和上面一样
    size_t end = strlen(buf);                               // buf的有效长度
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port); // buf+end怎么能加呢？这里面有一个拼接的把戏，控制不了n，没加长度
    return buf;
}

uint16_t InetAddress::port() const//lookat lookat 这里是toPort
{
    return ntohs(addr_.sin_port);
}

// #include <iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
//     return 0;
// }