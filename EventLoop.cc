#include "Poller.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <errno.h>

// #include <unistd.h> lookat 这三行源代码有，我是否要加上
// #include <fcntl.h>
// #include <memory>


// 防止一个线程创建多个eventloop  __thread就是thread local这么一个机制，每个线程都有一个
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义了默认的Poller io接口复用的超时时间
const int kPollTimeMs = 10000;

// 创建eventfd，用来唤醒subreactor来处理新channel??为什么卸载这里啊，奇奇怪怪的函数
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 设置的是非阻塞，执行fork或者exec后都不会继承父进程的打开状态
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}
EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)) // wakeupFd_只是注册了一个fd，还没有设置他感兴趣的事件
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    // 设置wakeup的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this)); // 绑定器和函数对象，必须熟悉
    // 每一个eventloop都将监听wakeupchannel的epollin读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    // 资源回收了，智能指针都要那个
    wakeupChannel_->disableAll(); // 置成都不感兴趣
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true; // 为什么叫事件循环？
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd 1、client的fd  2、wakeup的fd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); // 填进了活跃的channel
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生事件，然后上报给eventloop，通知channel处理相应事件
            channel->handleEvent(pollReturnTime_); // 叫起来
        }
        // 执行当前eventloop事件循环需要处理的回调操作
        /**
         * IO线程 mainloop accept fd 《= channel subloop
         * mainloop 事先注册一个回调cb（需要subloop来执行）    wakeup subloop后执行下面的方法，之前mainloop注册的cb操作，一个cb或者多个cb都可能
         *
         */
        doPendingFunctors(); // 执行回调
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 1、loop在自己线程中调用quit 2、在非loop的线程中，调用了loop的quit
void EventLoop::quit()
{
    quit_ = true;

    // 可能是帮别人结束
    if (!isInLoopThread()) // 2、在其他线程中，调用quit，在一个subloop（worker）里调用了mainloop（io）的quit
    {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else // 在非当前loop线程中执行cb
    {
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回到操作的loop的线程了
    //||callingPendingFunctors_意思是当前loop正在执行回调，但是loop又有了新的回调。
    if (!isInLoopThread() || callingPendingFunctors_)
    {             // 什么逻辑待解释？？
        wakeup(); // 唤醒loop所在线程
    }
}

// 写一个数据进wakeupfd，wakeupchannel发生读事件，当前loop被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1; // 发啥不重要
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{
    uint64_t one = 1; // 发啥不重要
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8", n);
    }
}

// 执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); // 当前loop的回调操作
    }

    callingPendingFunctors_ = false;
}
