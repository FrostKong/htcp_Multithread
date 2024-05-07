#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// channel未添加到poller中
const int kNew = -1; // channel的index_就是指这个
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop) // epoll_create1：系统的函数，选择了一个类型EPOLL_CLOEXEC应该是程序映像？
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

// 这里实现的是epoll_wait
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 使用log debug更合适，因为并发比较多
    LOG_INFO("func=%s -> fd total count=%d\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs); //&*拿到起始地址，单单那样不建议用，第三个强转一下，因为epollwait需要的是int的类型，这里的events_存放的是发生变化的events
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
        { // 要是超过了怎么办?
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s TIMEOUT!\n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR) // 这里就是记录一下
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// channel update remove -> eventloop updatechannel ->poller updatechannel
/**
 *          eventloop =>poller.poll
 *    channellist   poller
 *                  channelmap  <fd,channel*> epollfd
 * channellist里的channel大于等于channelmap里的，channelmap里是注册过的
 */
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s -> fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel; // 注册一下
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel); // 使用系统函数进行操作
    }
    else // channel已经注册过
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        { // 对所有事情都不感兴趣
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted); // 状态也改写一下
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s -> fd=%d\n", __FUNCTION__, channel->fd());

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel); // 在epoll里删除
    }
    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr); // 就是把epollwait里的东西都移到channel里
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // 这样子就能返给eventloop了，poller返给他的
    }
}

// epoll_ctl的mod/del/add
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel; // channel对应的就是ptr，绑定更新一下

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl mod/add error:%d\n", errno);
        }
    }
}
