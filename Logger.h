#pragma once

#include "noncopyable.h"
#include <string>
// 定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

// LOG_INFO("%s %d",arg1,arg2) 字符串s，int，d
#define LOG_INFO(logmsgFormat, ...)                                                             \
    do                                                                                          \
    {                                                                                           \
        Logger &logger = Logger::instance(); /*还有logger这里&用的引用是干嘛的呢*/ \
        logger.setLogLevel(INFO);            /*这里的info竟然能默认是int吗*/          \
        char buf[1024] = {0};                                                                   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); /*这不会编译器不认吧无语*/ \
        logger.log(buf);                                                                        \
    } while (0)

// LOG_ERROR("%s %d",arg1,arg2) 字符串s，int，d
#define LOG_ERROR(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(ERROR);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

// LOG_FATAL("%s %d",arg1,arg2) 字符串s，int，d
#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0)

#ifdef MODEBUG
// LOG_DEBUG("%s %d",arg1,arg2) 字符串s，int，d
#define LOG_DEBUG(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(DEBUG);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

class Logger : noncopyable
{
public:
    // 唯一实例
    static Logger &instance();
    // 设置logger等级
    void setLogLevel(int level);
    // 设置日志
    void log(std::string msg);

private:
    int logLevel_;
    Logger(){};
};