#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include <vector>
#include <unordered_map>
class Channel;
class EventLoop;

// muduo 多路复用分发器的核心IO复用模块
class Poller : noncopyable // 面向接口的编程
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);//
    // 虚析构函数 在继承结构中重要 可能因为析构的东西完全不一样，所以作成基类虚函数好处理
    virtual ~Poller() = default;

    // 给所有IO复用保留统一接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断channel是否在当前poller里
    bool hasChannel(Channel *channel) const; // 怎么不作成virtual？

    // 向单例模式一样获取一个具体的实例，默认的实例，获取IO复用的默认实现对象
    static Poller *newDefaultPoller(EventLoop *loop); // poll,epoll都可能获取到

protected:
    // map的key：sockfd value：fd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_; // 这可以拿到所有channel
private:
    EventLoop *ownerLoop_;
};