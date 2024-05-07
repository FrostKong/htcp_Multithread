#pragma once

// noncopyable被继承后，派生类对象能构造西沟，无法拷贝构造和赋值

class noncopyable{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&)=delete;
protected:
    noncopyable()=default;
    ~noncopyable()=default;
};