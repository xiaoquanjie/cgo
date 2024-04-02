/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/10
//----------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include <atomic>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <stdexcept>

namespace cgo::channel {
    struct channull {};

    struct _i_chan_st_ {
        virtual ~_i_chan_st_() = default;
        virtual bool recv(void* v) = 0;
        virtual bool send(const void* v) = 0;
        virtual void close() = 0;
    };

    std::shared_ptr<_i_chan_st_> make_chan(int,
                                           void(*free)(void*),
                                           void*(*malloc)(const void*),
                                           void(*copy)(void*, const void*));

    template<typename T, bool IS_TRIVIAL>
    struct chan_data {};

    template<typename T>
    struct chan_data<T, true> {
    protected:
        inline static void data_free(void*v) {
            free(v);
        }
        inline static void* data_malloc(const void* v) {
            void* nv = malloc(sizeof (T));
            memcpy(nv, v, sizeof (T));
            return nv;
        }
        inline static void data_copy(void* dst, const void* src) {
            memcpy(dst, src, sizeof (T));
        }
    };

    template<typename T>
    struct chan_data<T, false> {
    protected:
        inline static void data_free(void*v) {
            T* p = (T*)v;
            delete p;
        }
        inline static void* data_malloc(const void* v) {
            return (void*)new T(*((T*)v));
        }
        inline static void data_copy(void* dst, const void* src) {
            *(T*)dst = *(T*)src;
        }
    };

    // make sync data simple
    template<typename T>
    struct chan  : public chan_data<T, std::is_trivially_copyable<T>::value> {
        template<class A>
        friend chan<A> makeChan(int);

        template<typename A>
        friend void closeChan(const chan<A>&);

        // output
        inline bool operator >> (T& v) const {
            return recv(&v);
        }
        // output
        inline bool operator >> (const channull&) const {
            return recv(0);
        }
        // input
        inline bool operator << (const T& v) const {
            return send(&v);
        }

        [[nodiscard]]
        inline size_t use_count() const {
            return _ch ? _ch.use_count() : 0;
        }
    protected:
        inline bool recv(T* v) const {
            if (!_ch) {
                throw std::runtime_error("chan is nil");
            }
            return _ch->recv(v);
        }

        inline bool send(const T* v) const {
            if (!_ch) {
                throw std::runtime_error("chan is nil");
            }
            return _ch->send(v);
        }
    private:
        std::shared_ptr<_i_chan_st_> _ch;
    };

    template<typename T>
    inline chan<T> makeChan(int cap = 0) {
        chan<T> ch;
        ch._ch = make_chan(cap, ch.data_free, ch.data_malloc, ch.data_copy);
        return ch;
    }

    template<typename T>
    void closeChan(const chan<T>& c) {
        if (c._ch) {
            c._ch->close();
        }
    }
}

namespace cgo {
    using channel::chan;
}

#undef makechan
#define makechan cgo::channel::makeChan

#undef closechan
#define closechan cgo::channel::closeChan

#undef channull
#define channull cgo::channel::channull()