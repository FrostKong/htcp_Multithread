#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "Logger.h"
#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
#include "Channel.h"
class Poller;
// 事件循环类型，主要包含两大模块channel、Poller（epoll的抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行回调cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

    // 唤醒loop所在线程
    void wakeup();

    // eventloop->poller
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断eventloop是否在自己线程里边
    bool isInLoopThread() const
    {
        return threadId_ == CurrentThread::tid();
    }

private:
    // wake up
    void handleRead();
    void doPendingFunctors(); // 执行回调
    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_; // 原子操作的布尔值，通过CAS实现（自旋锁）
    std::atomic_bool quit_;    // 标志退出loop循环

    const pid_t threadId_;
    // 当前loop所在线程id，worker的eventloop都监听了一组的channel，判断是否在自己的线程里边
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; // 有mainloop还有很多subloop，当mainloop获取到一个新用户的channel时，通过轮询算法找一位subloop处理，通过该变量唤醒这位subloop，eventfd在网上多查查资料!!
    // 每一个loop都有一个wakeupfd
    std::unique_ptr<Channel> wakeupChannel_; // 监视上方的这个

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_; // 当前loop是否有需要执行的回调
    std::vector<Functor> pendingFunctors_;    // 存储当前loop所需要执行所有回调，只在自己的loop里执行回调
    std::mutex mutex_;                        // 互斥锁，保护上面vector线程安全操作
};