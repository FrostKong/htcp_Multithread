#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);
Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)), // 这是浅拷贝，为什么不行
      name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    // 挑选什么参数进行析构?
    if (started_ && !joined_)
    {
        thread_->detach(); // join是工作线程和detach守护线程冲突
    }
}

void Thread::start()
{
    started_ = true;
    sem_t sem;// 这有什么用吗？
    sem_init(&sem, false, 0);
    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]()
                                                           { 
        // 获取线程的tid值
        tid_= CurrentThread::tid();
        sem_post(&sem);
        //开启一个新线程，专门执行该线程函数
        func_(); }));
    // 这是个lambda，开启一个新线程，专门执行该线程函数

    // 这里等待获取上面新创建线程的tid
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32];//lookat 这里源代码={0}
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
