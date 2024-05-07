//#pragma once   warning: #pragma once in main file [enabled by default]
#include <iostream>
#include "Logger.h"
#include "Timestamp.h"
Logger &Logger::instance() // 这个&可能放左放右都一样。
{
    static Logger logger;//线程安全的
    return logger; // 引用，能返回实例，*，不能返回实例
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}
//写日志：[级别信息]time：日志信息
void Logger::log(std::string msg) // 为什么这里显示不了
{
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    // 打印时间和msg
    std::cout << Timestamp::now().toString()
              << ":" << msg << std ::endl;
}
