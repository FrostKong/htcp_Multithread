#include "mymuduo/TcpServer.h"
#include "mymuduo/Logger.h"

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const std::string &name)
        : server_(loop, listenAddr, name),
          loop_(loop)
    {
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        LOG_INFO("EchoServer - %s -> %s is %s", conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str(), (conn->connected() ? "UP" : "DOWN"));
    }

    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp time)
    {
        std::string msg(buf->retrieveAllAsString());
        conn->send(msg);
        conn->shutdown();
    }

    TcpServer server_;
    EventLoop *loop_;
};

int main()
{
    EventLoop loop;
    InetAddress listenAddr(8000);
    EchoServer server(&loop, listenAddr, "echoserver-hans");
    server.start();
    loop.loop();
    return 0;
}