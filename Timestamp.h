#pragma once
#include <string>
#include <iostream>
#include <stdint.h>
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);//构造函数加了explicit，防止了隐式转换
    static Timestamp now();
    std::string toString() const;//这个const对的是哪个
private:
    //__INT64_TYPE__
    int64_t microSecondsSinceEpoch_;
};