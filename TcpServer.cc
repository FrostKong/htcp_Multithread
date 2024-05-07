#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"
#include <functional>
#include <strings.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop) // tcpconnection里面也有，static防止编译冲突
{
    if (loop == nullptr)//为啥我老是没有加==，lookat
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)), // 为空会出大事，需要检测一下
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option)),  //源代码里默认的是kreuseport
      threadPool_(new EventLoopThreadPool(loop, nameArg)),
      connectionCallback_(), // defaultConnectionCallback
      messageCallback_(),    // defaultMessageCallback
      nextConnId_(1),        // 为什么是1?
      started_(0)
{
    // 当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); // 出了这个局部，shared——ptr能直接释放。
        item.second.reset();

        // 这样子的话，这一句话也能调用，并销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// loop.loop()
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个tcpserver对象被start多次，started是否为0，然后再加1
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(
            std::bind(&Acceptor::listen, acceptor_.get())); // get拿到一个裸指针，开始用注册到poller
    }
}

// 有一个新的客户端的连接，acceptor会执行这个回调操作,acceptor的handleread里，tcpserver的构造里面设置了这个回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 轮询算法，选择一个subloop，来管理这个channel。组装了这个连接的名字
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n", name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getlocaladdr");
    }

    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建tcpconnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    //  下面的回调都是用户设置给tcpserver-》tcpconnection-》channel-》poller-》notify channel
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调 conn->shutdown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用tcpconnection connectionestablish
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("Tcpserver::removeConnectioninloop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name()); //lookat 这里sizet在源代码里没有
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}
