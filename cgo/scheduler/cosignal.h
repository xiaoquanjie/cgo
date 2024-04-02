//
// Created by xiaoqj on 2024/1/26.
//

#pragma once

namespace cgo {
    class co_signal {
        unsigned long long _id = 0;
        void* _sig = nullptr;

    public:
        [[nodiscard]]
        unsigned long long id() const;

        void init();
        void init(unsigned long long id);

        void wait(void*&data);

        void wait();

        void post(void*data);

        void post();

        // 需要手动释放，否则会内存泄露
        void close();
    };

    using signal = co_signal;
}
