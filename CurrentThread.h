#pragma once
#include <sys/syscall.h>
#include <unistd.h>
namespace CurrentThread
{
    extern __thread int t_cachedTid; // 头文件里extern一下

    void cacheTid();

    inline int tid()//这里使用了内联函数
    {
        if (__builtin_expect(t_cachedTid == 0, 0)) // 还没获取过当前线程的id
        {
            cacheTid();
        }
        return t_cachedTid;
    }

}