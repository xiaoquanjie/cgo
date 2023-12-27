//
// Created by xiaoqj on 2023/5/15.
// 提供各种萃取器
//

#pragma once

#include <functional>
#include <tuple>
#include <stdint.h>

template<class T>
struct function_traits;

template<class R, class ...Args>
struct function_traits<std::function<R(Args...)>> {
    static const size_t arity = sizeof...(Args);

    typedef R result_type;

    template<size_t i>
    struct arg_type {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    };
};

template<class R, class ...Args>
struct function_traits<R(Args...)> {
    static const size_t arity = sizeof...(Args);

    typedef R result_type;

    template<size_t i>
    struct arg_type {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    };
};

template<class R, class ...Args>
struct function_traits<R(*)(Args...)> {
    static const size_t arity = sizeof...(Args);

    typedef R result_type;

    template<size_t i>
    struct arg_type {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    };
};

template<class T, class R, class ...Args>
struct function_traits<R(T::*)(Args...)> {
    static const size_t arity = sizeof...(Args);

    typedef R result_type;

    template<size_t i>
    struct arg_type {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    };
};


#define PRIVATE_MACRO_VAR_ARGS_IMPL_COUNT(_1,_2,_3,_4,_5,_6,_7,_8,_9, N, ...) N
#define PRIVATE_MACRO_VAR_ARGS_IMPL(args)  PRIVATE_MACRO_VAR_ARGS_IMPL_COUNT args
#define COUNT_MACRO_VAR_ARGS(...)  PRIVATE_MACRO_VAR_ARGS_IMPL((__VA_ARGS__,10,9,8,7,6,5 4,3,2,1,0))

#define PRIVATE_MACRO_CHOOSE_HELPER2(M,count)  M##count
#define PRIVATE_MACRO_CHOOSE_HELPER1(M,count)  PRIVATE_MACRO_CHOOSE_HELPER2(M,count)
#define PRIVATE_MACRO_CHOOSE_HELPER(M,count)   PRIVATE_MACRO_CHOOSE_HELPER1(M,count)