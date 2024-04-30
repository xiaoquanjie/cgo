//
// 时间轮实现
// 轮长：1分钟
// 精度：1毫秒
// 多层设计：最大支持12层，时间跨度59分59秒999毫秒
// Created by xiaoqj on 2024/4/29.
//

#pragma once

#include <cstdint>
#include <chrono>
#include <vector>

namespace timer {
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;

    class Timer {
    public:
        struct tnode {
            uint64_t id = 0;
            uint64_t payload = 0;
        };

        using tnode_list = std::vector<tnode>;

        struct TimerInfo {
            static const uint16_t _sWheelLen = 1 * 60 * 1000;
            static const uint8_t _sWheelLevel = 60;
            tnode_list **_wheel[_sWheelLen];
            uint64_t _beg;
            uint64_t _now;
            uint32_t _id = 1;
            uint32_t _count = 0;
            void (*_notify)(uint64_t, uint64_t) = nullptr;
        };

    protected:
        TimerInfo _info;

        explicit Timer(void (*notify)(uint64_t, uint64_t));

    public:
        static Timer* NewTimer(void (*notify)(uint64_t, uint64_t));

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        virtual ~Timer();

        [[nodiscard]]
        uint32_t count() const;

        // @interval是毫秒
        // @返回值为定时器id,返回0说明添加定时器失败，原因应该是interval过大，超出最大时间间隔
        uint64_t add(uint32_t interval, uint64_t payload);

        void update();
    };

    Timer* NewTimer(void (*notify)(uint64_t, uint64_t));
}

