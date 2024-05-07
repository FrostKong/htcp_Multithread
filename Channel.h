#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>
// #include <EventLoop.h>
//  channel封装了sockfd和感兴趣的事件events，epollin、epollout，还绑定了poller返回的具体的事情revents

class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); } // move将左值强制转化为右值，节省资源
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止channel被手动remove掉，channel还在执行回调函数
    void tie(const std::shared_ptr<void> &); // 绑定了，不知道为何绑定？

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态
    void enableReading()
    {
        events_ |= kReadEvent; // k可能代表内核
        update();
    } // 按位或
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    } // 按位与，~取反
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; } // 为什么
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop *ownerLoop() { return loop_; }
    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent; // 这里不要定义，要不然其他文件重复定义
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;
    const int fd_;    // fd，poller监听对象
    int events_;      // fd感兴趣的事件
    int revents_;     // poller返回具体的事件
    int index_;

    std::weak_ptr<void> tie_; // 绑自己，shared from this，看一下博客
    bool tied_;

    // channel里面能获知fd最终发生的事情revents，负责调用具体事件的回调
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};