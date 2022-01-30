#pragma once

template<typename T>
class MeyersSingleton
{
private:
    MeyersSingleton() = default;  // 设置为私有(protected也行)，禁止外部直接构造

public:
    MeyersSingleton(const MeyersSingleton&) = delete; // 禁止拷贝构造
    MeyersSingleton& operator = (const MeyersSingleton&) = delete; // 禁止拷贝赋值
    ~MeyersSingleton() = default;

public:
    static T& Instance() //  Meyers' Singleton 
    {
        static T instance;
        return instance;
    }
};
