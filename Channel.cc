#include "Channel.h"
#include "Logger.h"
#include "EventLoop.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;                  // static为什么不见了，可能这里申明了用不了，和头文件里的不一样
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // EPOLLPRI:高优先级的带外数据可读
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
}

// channel的tie方法是什么时候调用的？：一个tcpconnection新连接创建的时候
// tcpconnection->channel 考虑到tcpconnection如果被remove掉，这里就能观察到
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

// 在eventloop里，把当前channel删掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

/**
 *channel->eventloop->poller，去到poller里面改变fd相应的事件epoll_ctl。
 *eventloop =》两个孩子：channelList Poller
 */
void Channel::update()
{
    loop_->updateChannel(this);
}

// (channel核心-》handleEventWithGuar)fd得到poller通知后，处理事件
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_) // 是否绑定过，监听过一个channel
    {
        std::shared_ptr<void> guard = tie_.lock(); // lock检查是否有强智能指针，有的话返回。
        if (guard)
        { // 如果存活，继续处理
            handleEventWithGuard(receiveTime);
        }
    }
    else
    { // 没有绑定过，继续处理
        handleEventWithGuard(receiveTime);
    }
}

// poller通知channel发生的具体事件，channel负责调用回调
void Channel::handleEventWithGuard(Timestamp receiveTime)
{ // revents_在哪里进行修改？
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) // EPOLLHUP 表示读写都关闭 ,lookat这里有个！，源代码里
    {
        if (closeCallback_)
        {
            closeCallback_(); // 一个回调
        }
    }
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    if (revents_ & (EPOLLIN | EPOLLPRI)) // EPOLLPRI 外带数据
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
