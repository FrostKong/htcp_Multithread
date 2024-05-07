#pragma once
#include <functional>
#include <thread>
#include <memory>
#include <string>
#include <unistd.h>
#include <atomic>

#include "noncopyable.h"

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>; // 返回值是void不带参数的函数，如果想要参数，可以参考绑定器和函数对象，比较热门记得看！！！

    explicit Thread(ThreadFunc func, const std::string &name = std::string()); // 这是啥 lookat 源代码没有func
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_; // 用智能指针封装一下，因为这个会马上启动
    pid_t tid_;                           // 不是unistd里面的吗
    ThreadFunc func_;
    std::string name_;
    // 静态成员变量单独在类外进行初始化
    static std::atomic_int numCreated_;
};