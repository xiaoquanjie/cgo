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
        struct _i_chan_st_ {
            virtual ~_i_chan_st_() {}
            virtual bool recv(void* v) = 0;
            virtual bool send(const void* v) = 0;
            virtual void close() = 0;
        };

        _i_chan_st_* make_chan(int,
                               void(*destructor)(void*),
                               void*(*constructor)(),
                               void(*copy)(void*, const void*));

        // make sync data simple
        template<typename T>
        struct chan {

            template<class A>
            friend chan<A> makeChan(int);

            template<typename A>
            friend void closeChan(const chan<A>&);

            chan() {
                _ref = new std::atomic_int;
                _ref->store(0, std::memory_order_relaxed);
                incr_ref();
            }

            chan(const chan<T>& other) {
                copy(other);
            }

            chan(chan<T>&& other) {
                copy(other);
            }

            chan& operator=(const chan<T>& other) {
                desc_ref();
                copy(other);
                return *this;
            }

            chan& operator=(chan<T>&& other) {
                desc_ref();
                copy(other);
                return *this;
            }

            ~chan() {
                desc_ref();
            }

            // not allow to call in non-coroutine
            inline bool operator << (T& v) {
                return recv(v);
            }

            inline bool operator << (T& v) const {
                return recv(v);
            }

            // not allow to call in non-coroutine
            inline bool operator >> (const T& v) {
                return send(v);
            }

            inline bool operator >> (const T& v) const {
                return send(v);
            }

			inline size_t use_count() const {
				return *_ref;
			}

			inline size_t use_count() {
                return *_ref;
			}
        protected:
            inline bool recv(T& v) const {
                if (!_ch) {
                    throw "chan is nil";
                }

                if (_ch->recv(&v)) {
                    return true;
                }
                return false;
            }

            inline bool send(const T& v) const {
                if (!_ch) {
                    throw "chan is nil";
                }

                if (!_ch->send(&v)) {
                    return false;
                }
                return true;
            }

            inline void desc_ref() {
                if (_ref->fetch_sub(1, std::memory_order_relaxed) == 1) {
                    delete _ch;
                    delete _ref;
                    _ch = 0;
                    _ref = 0;
                }
            }

            inline void incr_ref() {
                _ref->fetch_add(1, std::memory_order_relaxed);
            }

            inline void copy(const chan<T>& other) {
                this->_ref = other._ref;
                this->_ch = other._ch;
                incr_ref();
            }

            inline static void data_destructor(void*v) {
                T* p = (T*)v;
                delete p;
            }

            inline static void* data_constructor() {
                return (void*)new T;
            }

            inline static void data_copy(void* dst, const void* src) {
                *(T*)dst = *(T*)src;
            }
        private:
            _i_chan_st_ *_ch = 0;
            std::atomic_int* _ref = 0;
        };

        template<typename T>
        inline chan<T> makeChan(int cap = 0) {
            chan<T> ch;
            ch._ch = make_chan(cap, ch.data_destructor, ch.data_constructor, ch.data_copy);
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