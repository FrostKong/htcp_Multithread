#include "Buffer.h"
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

/**
 * 从fd上读取数据，Poller工作在LT模式
 * Buffer缓冲区是有大小的，但是从fd上读数据，却不知道tcp数据最终的大小
 */
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间，这能自动回收？64k
    struct iovec vec[2];        // readv系统函数能放数据到分离的多个缓冲区
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; // 最多能读64k，要是小于64k，那就拿两块儿来写
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable)
    {
        writerIndex_ += n;
    }
    else
    { // extrabuf里面也写入数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes()); // 不用写this
    if (n < 0)                                        // 在外面判断=0和>0
    {
        *saveErrno = errno;
    }
    return n;
}
