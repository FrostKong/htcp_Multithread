#include "EventLoopThread.h"
#include "EventLoop.h"
// 怎么实现one loop per thread
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(), // 默认构造
      cond_(),  // 默认构造
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit(); // 绑定的事件循环
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock);//我感觉这里不太对，稍微太复杂了一点，但是我感觉我自己的也很复杂，不知道哪个更对一点
        }
        //后续
        loop = loop_;
    }
    return loop;
}

// 下面这个方法，是在另一个（我感觉）单独的新线程里运行
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的eventloop，和上面的线程一一对应，one loop per thread

    if (callback_)//这里应该就是运行一下
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // eventloop loop=> poller poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
