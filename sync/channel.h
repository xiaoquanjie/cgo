/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/10
//----------------------------------------------------------------*/

#pragma once

#include <stdint.h>
#include <memory>
#include <assert.h>

namespace cgo {
    namespace channel {
        struct _i_chan_st_ {
            virtual ~_i_chan_st_() {}
            virtual bool read(std::shared_ptr<void>& v) = 0;
            virtual bool write(std::shared_ptr<void>& v) = 0;
        };

        std::shared_ptr<_i_chan_st_> make_chan(int);

        // make sync data simple
        template<typename T>
        struct chan {
            template<class A>
            friend chan<A> makechan(int);

            // not allow to call in non-coroutine
            inline void operator << (T& v) {
                if (!_ch) {
                    assert(0==1 && ("chan is nil"));
                    return;
                }

                std::shared_ptr<void> pv;
                if (_ch->read(pv)) {
                    auto tv = std::static_pointer_cast<T>(pv);
                    v = *tv.get();
                }
            }

            // not allow to call in non-coroutine
            inline void operator >> (const T& v) {
                if (!_ch) {
                    assert(0==1 && ("chan is nil"));
                    return;
                }

                auto pv = std::make_shared<T>(v);
                _ch->write((std::shared_ptr<void>&)pv);
            }
        private:
            std::shared_ptr<_i_chan_st_> _ch;
        };

        template<typename T>
        inline chan<T> makechan(int cap = 0) {
            chan<T> ch;
            ch._ch = make_chan(cap);
            return ch;
        }
    }

    using channel::chan;
}

#define makechan cgo::channel::makechan