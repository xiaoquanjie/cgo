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
#include <assert.h>

namespace cgo {
    namespace channel {
        struct _i_chan_st_ {
            virtual ~_i_chan_st_() {}
            virtual bool read(void*& v) = 0;
            virtual bool write(void* v) = 0;
            virtual void close() = 0;
        };

        std::shared_ptr<_i_chan_st_> make_chan(int, const std::function<void(void*)>&);

        // make sync data simple
        template<typename T>
        struct chan {
            template<class A>
            friend chan<A> makeChan(int);

            template<typename A>
            friend void closeChan(const chan<A>&);

            // not allow to call in non-coroutine
            inline bool operator << (T& v) {
                return read(v);
            }

            inline bool operator << (T& v) const {
                return read(v);
            }

            // not allow to call in non-coroutine
            inline bool operator >> (const T& v) {
                return write(v);
            }

            inline bool operator >> (const T& v) const {
                return write(v);
            }

			inline size_t use_count() const {
				if (_ch) {
					return _ch.use_count();
				}
				return 0;
			}

			inline size_t use_count() {
				if (_ch) {
					return _ch.use_count();
				}
				return 0;
			}
        protected:
            inline bool read(T& v) const {
                if (!_ch) {
                    throw "chan is nil";
                }

                void* pv = 0;
                if (_ch->read(pv)) {
                    v = *(T*)pv;
                    _destructor(pv);
                    return true;
                }
                return false;
            }

            inline bool write(const T& v) const {
                if (!_ch) {
                    throw "chan is nil";
                }

                auto pv = new T(v);
                if (!_ch->write(pv)) {
                    _destructor(pv);
                    return false;
                }
                return true;
            }

        private:
            std::shared_ptr<_i_chan_st_> _ch;
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