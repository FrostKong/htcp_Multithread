#include "CurrentThread.h"

namespace CurrentThread//namespace怎么说
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            // 通过系统调用获取当前线程id，获取一下放进来
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}