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
            virtual bool recv(void*& v) = 0;
            virtual bool send(void* v) = 0;
            virtual void close() = 0;
        };

        _i_chan_st_* make_chan(int, const std::function<void(void*)>&);

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

            chan(chan<T>& other) {
                copy(other);
            }

            chan(chan<T>&& other) {
                copy(other);
            }

            chan& operator=(chan<T>& other) {
                copy(other);
                return *this;
            }

            chan& operator=(chan<T>&& other) {
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

                void* pv = 0;
                if (_ch->recv(pv)) {
                    if (pv) {
                        v = *(T*)pv;
                        _destructor(pv);
                    }
                    return true;
                }
                return false;
            }

            inline bool send(const T& v) const {
                if (!_ch) {
                    throw "chan is nil";
                }

                auto pv = new T(v);
                if (!_ch->send(pv)) {
                    _destructor(pv);
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

            inline void copy(chan<T>& other) {
                desc_ref();

                this->_ref = other._ref;
                this->_ch = other._ch;
                incr_ref();
            }
        private:
            _i_chan_st_ *_ch = 0;
            std::atomic_int* _ref = 0;
            static std::function<void(void*)> _destructor;
        };

        template<typename T>
        std::function<void(void*)> chan<T>::_destructor = [](void*t) {
            T* p = (T*)t;
            delete p;
        };

        template<typename T>
        inline chan<T> makeChan(int cap = 0) {
            chan<T> ch;
            ch._ch = make_chan(cap, ch._destructor);
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