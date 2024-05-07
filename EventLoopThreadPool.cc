#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // 线程里面绑定的loop都是stack上面的，所以不用管，会一直在运行。
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size()+32];
        snprintf(buf,sizeof buf,"%s%d",name_.c_str(),i);
        EventLoopThread *t=new EventLoopThread(cb,buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));//手动帮我们释放
        loops_.push_back(t->startLoop());//底层创建线程，绑定一个新的Eventloop，并返回该loop的地址
    }
    // 整个服务端只有一个线程baseloop在运行
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
    //轮询
    EventLoop* loop=baseLoop_;

    if(!loops_.empty()){
        loop=loops_[next_];
        ++next_;
        if(next_>=loops_.size()){
            next_=0;
        }
    }
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if(loops_.empty()){
        return std::vector<EventLoop*>(1,baseLoop_);
    }
    else{
        return loops_;//lookat 这里源代码忘记写return了
    }
}
