#include "TcpConnection.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <functional>
#include <memory>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop *CheckLoopNotNull(EventLoop *loop) // tcpconnection里面也有，static防止编译冲突
{
    if (loop == nullptr) // 哈哈，lookat 这里错误改过来了
    {
        LOG_FATAL("%s:%s:%d tcpconnection loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting), // 初始状态给的这个
      reading_(true),      // 给的true？
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64M
{
    // 注意和acceptor进行对比，这里注册的东西更多，但都是类似的
    // 下面给channel设置相应的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd); //ctor-constructor
    socket_->setKeepAlive(true); // 保活机制
}

TcpConnection::~TcpConnection() // 没有什么可以需要析构的
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
}

// 这两个函数也是我们上层tcpserver绑定调用的
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this()); // 这里是绑定了还是执行了 ，handleclose里也用了？？？
}

void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel所有感兴趣的事件，从poller中del掉
    }
    channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno); // 拿出socket_的fd，都是一样的，在构造函数里用的一样的fd构造的
    if (n > 0)                                                    // 有数据
    {
        // 已连接用户，有可读事件发生了，调用用户传入
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime); // 这个shared为啥不用加std，还是在memory里但是没有名字空间作用域，不用加::
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("tcpconnection::handleread");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting()) // 先判断一下是否在写
    {
        int savedErrno = 0;
        // ssize_t n = sockets::write(channel_->fd(),
        //                        outputBuffer_.peek(),
        //                        outputBuffer_.readableBytes());
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno); // 封装一下，不要直接用socket::write()，细节不要暴露在tcpconnection里，buffer里的写到fd里面去
        if (n > 0)
        {
            outputBuffer_.retrieve(n);              // 读完以后清空n个
            if (outputBuffer_.readableBytes() == 0) // 如果拿完数据了
            {
                channel_->disableWriting(); // 设置对写不感兴趣
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())); // 用queueinloop是因为，要唤醒loop对应thread线程，？为什么用，当前线程执行回调？
                }
                if (state_ == kDisconnecting)
                {                     // 正在关闭
                    shutdownInLoop(); // 当前loop删掉tcpconnection
                }
            }
        }
        else
        {
            LOG_ERROR("Tcpconnection::handlewrite");
        }
    }
    else
    {
        LOG_ERROR("tcpconnection fd=%d is down,no more writing \n", channel_->fd());
    }
}

// poller->channel::closecallback ->tcpconnection::handleclose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll(); // 所有事件都不感兴趣了

    // 通知用户的onconnection
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 这边没判断就直接调用了，没有判断是否有效 ,执行连接关闭的回调
    closeCallback_(connPtr);      // 关闭连接的回调（其实稍微和上面重复了） 执行的是tcpserver的removeconnection
}

void TcpConnection::handleError()
{ // 仿佛if else了寂寞，用于打印不同出错消息
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) // soerror是一个现代的方法，让你看到错误原因，getsockopt成功返回0，错误返回-1
    {
        err = errno; // getsockopt设置了，出错了
    }
    else // 成功返回0
    {
        err = optval;
    }
    LOG_ERROR("tcpconnection::handleerror name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

// 传的是string，不是buffer，buffer调用方自己去管
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected) // 都是执行sendinloop
    {
        if (loop_->isInLoopThread()) // 这里我感觉没有必要进行判断，因为runinloop里面已经有判断了 lookat 这里是怎么判断的
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(
                std::bind(
                    &TcpConnection::sendInLoop,
                    this,
                    buf.c_str(),
                    buf.size()));
        }
    }
}

/**
 * 发送数据 应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调。可能是卡在路上了，这个函数和handlewrite有什么区别
 */
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0; // 这俩类型不一样！
    size_t remaining = len;
    bool faultError = false;

    // 如果之前调用过该connection的shutdown，就不能再进行发送了
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected,give up writing!");
        return;
    }

    // 表示channel_第一次开始写数据，并且缓冲区没有待发送数据。
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) // 是否不对写事件感兴趣
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;                     // 还剩多少没有发送
            if (remaining == 0 && writeCompleteCallback_) // 如果都发完了
            {
                // 既然在这里一次性数据全部发送完成，就不用再给channel设置epollout事件了。而有epollout事件的时候才执行handlewrite（他关注的是epollout）。
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
            else
            { // nwrote<0
                nwrote = 0;
                if (errno != EWOULDBLOCK)//EWOULDBLOCK代表缓冲区没有数据，返回错误
                {
                    LOG_ERROR("Tcpconnection::sendinloop");
                    if (errno == EPIPE || errno == ECONNRESET) // 链接重置
                    {
                        faultError = true;
                    }
                }
            }
        }
    }
    // 说明当前这一次write没有把全部数据发出。需要保存到缓冲区域当中。
    // 给channel注册epollout事件，LT模式，poller会发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用handlewrite回调方法。把发送缓冲区的数据全部发送完成
    if (!faultError && remaining > 0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ &&
            oldLen < highWaterMark_ &&
            highWaterMarkCallback_) // 说明旧数据低于高水位，但是两种数据合并高于高水位
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining)); // 这里绑定了会发生什么？，可能会发生某些事情
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明outputbuffer中的数据已经全部发送完成了
    {
        socket_->shutdownWrite(); // 关闭写端。。。不知道为什么这里要对这个socket进行设置
    }
}
