#include "Timestamp.h"
#include "stdio.h"//奇怪，这种东西需要加吗
Timestamp::Timestamp() : microSecondsSinceEpoch_(0)
{
}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{
}

Timestamp Timestamp::now()
{
    return Timestamp(time(NULL)); // time是一个time.h里的函数，返回现在的时间，精度较低。
    //time(2) / time_t （秒）;gettimeofday(2) / struct timeval （微秒） 可以自行选择
}

std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_); // tm是一个time.h里的数据结构，可以分割出很多表现形式的时间样式;localtime能改变mi。。;&取地址
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d", tm_time->tm_year+1900, tm_time->tm_mon+1, tm_time->tm_mday, tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);//d宽度，0表示左边补0

    return buf;
}
// #include <iostream>
// int main()
// {
//     Timestamp times;
//     std::cout<<times.now().toString();
//     return 0;
// }