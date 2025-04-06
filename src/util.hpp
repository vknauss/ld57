#pragma once

#include <utility>

template<auto T>
struct InitShim {};

template<typename T, typename Ret, typename ... Args, Ret (T::*Fn) (Args...)>
struct InitShim<Fn>
{
    InitShim(T& obj, Args&& ... args)
    {
        (obj.*Fn)(std::forward<Args>(args)...);
    }
};

