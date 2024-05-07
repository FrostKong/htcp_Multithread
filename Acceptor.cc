#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0); // 有些：：前面有，有些前面没有
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;//lookat 这个源代码没有为啥
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()), // 较为复杂，创建了socket，封装成channel
      acceptChannel_(loop, acceptSocket_.fd()),
      listening_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);      // 设置reuseport和addr，可以让多进程或者多线程创建多个socket绑定到同一个PORT上
    acceptSocket_.bindAddress(listenAddr); // bind套接字
    // tcpserver：：start() acceptor.listen 有新用户连接，要执行一个回调connfd-》打包成channel给到subloop
    // baseloop监听到acceptchannel(listenfd)=>
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); // 怎么感觉回调的参数不太对呢：这里的this不是参数，是谁的handleread这里需要标注一下
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listening_ = true;
    acceptSocket_.listen();         // listen
    acceptChannel_.enableReading(); // acceptchannel注册到poller里面
}

// 在channel收到读事件的时候，新用户连接，
void Acceptor::handleRead()
{
    InetAddress peerAddr; // 没有默认构造，所以找到构造那里加一个0
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr); // 轮询，找到subloop，唤醒，分发当前的新客户端的channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) // 这里是fd用完了
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit err! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
