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
#include <functional>
#include <atomic>
#include <assert.h>

namespace cgo {
    namespace channel {
        struct channull {};

        struct _i_chan_st_ {
            virtual ~_i_chan_st_() {}
            virtual bool recv(void* v) = 0;
            virtual bool send(const void* v) = 0;
            virtual void close() = 0;
        };

        std::shared_ptr<_i_chan_st_> make_chan(int,
                                               void(*free)(void*),
                                               void*(*malloc)(const void*),
                                               void(*copy)(void*, const void*));

        // make sync data simple
        template<typename T>
        struct chan {
            template<class A>
            friend chan<A> makeChan(int);

            template<typename A>
            friend void closeChan(const chan<A>&);

            inline bool operator << (T& v) const {
                return recv(&v);
            }
            inline bool operator << (const channull&) const {
                return recv(0);
            }
            inline bool operator >> (const T& v) const {
                return send(&v);
            }

			inline size_t use_count() const {
				return _ch ? _ch.use_count() : 0;
			}
        protected:
            inline bool recv(T* v) const {
                if (!_ch) {
                    throw "chan is nil";
                }
                return _ch->recv(v) ? true : false;
            }

            inline bool send(const T* v) const {
                if (!_ch) {
                    throw "chan is nil";
                }
                return _ch->send(v) ? true : false;
            }

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

    using channel::chan;
}

#undef makechan
#define makechan cgo::channel::makeChan

#undef closechan
#define closechan cgo::channel::closeChan

#undef channull
#define channull cgo::channel::channull()