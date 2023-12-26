/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/10
//----------------------------------------------------------------*/

#include "channel.h"
#include "../scheduler/scheduler.h"
#include "../common/circle_queue.h"
#include "../common/slist.h"
#include <mutex>

namespace cgo {
    namespace channel {
        struct _wait_st_ {
            void* _data;
            uint64_t _co_id;
        };

        struct _chan_st_ : public _i_chan_st_ {
            bool _closed = false;
            cqueue<void*> _buf;
            void(*_destructor)(void*);
            void*(*_constructor)();
            void(*_copy)(void*, const void*);

            std::mutex _mu;
            slist<_wait_st_> _sendq;
            slist<uint64_t> _recvq;

            _chan_st_(int cap,
                      void(*destructor)(void*),
                      void*(*constructor)(),
                      void(*copy)(void*, const void*))
                : _buf(cap)
                , _destructor(destructor)
                , _constructor(constructor)
                , _copy(copy) {
            }

            ~_chan_st_() {
                this->close();
                for (void* data; _buf.pop(data);) {
                    _destructor(data);
                }
            }

            bool recv(void* v) {
                auto co_id = coro_adapter::cur_coid();
                if (co_id == M_INVALID_COROUTINE_ID) {
                    throw "not allow to read chan in non-coroutine";
                }

                _mu.lock();
                if (_closed) {
                    _mu.unlock();
                    return false;
                }

                for (_wait_st_ wait; _sendq.pop(wait);) {
                    scheduler:: schedule_co(wait._co_id, 0);
                    _mu.unlock();

                    _copy(v, wait._data);
                    _destructor(wait._data);
                    return true;
                }

                void* popdata = 0;
                if (!_buf.empty()) {
                    _buf.pop(popdata);
                    _mu.unlock();

                    _copy(v, popdata);
                    _destructor(popdata);
                    return true;
                }

                _recvq.push(co_id);
                scheduler::schedule_yield(popdata, [this]() {
                    _mu.unlock();
                });

                if (popdata) {
                    _copy(v, popdata);
                    _destructor(popdata);
                }

                if (_closed) {
                    return false;
                }
                return true;
            }

            bool send(const void* v) override {
                auto co_id = coro_adapter::cur_coid();
                if (co_id == M_INVALID_COROUTINE_ID) {
                    throw "not allow to write chan in non-coroutine";
                }

                _mu.lock();
                if (_closed) {
                    _mu.unlock();
                    return false;
                }

                void* newv = _constructor();
                _copy(newv, v);

                // if _buf is empty, _recvq.pop return true
                for (uint64_t wait; _recvq.pop(wait);) {
                    _mu.unlock();
                    scheduler::schedule_co(wait, newv);
                    return true;
                }

                if (!_buf.full()) {
                    _buf.push(newv);
                    _mu.unlock();
                    return true;
                }

                _sendq.push(_wait_st_{newv, co_id});

                void* nonedata = 0;
                scheduler::schedule_yield(nonedata, [this]() {
                     this->_mu.unlock();
                });

                if (_closed) {
                    return false;
                }
                return true;
            }

            void close() override {
                std::unique_lock<std::mutex> lock(this->_mu);
                if (this->_closed) {
                    return;
                }

                this->_closed = true;

                for (uint64_t co_id; _recvq.pop(co_id);) {
                    scheduler::schedule_co(co_id, 0);
                }

                for (_wait_st_ wait; _sendq.pop(wait);) {
                    scheduler::schedule_co(wait._co_id, 0);
                    _destructor(wait._data);
                }
            }
        };

        _i_chan_st_* make_chan(int cap,
                               void(*destructor)(void*),
                               void*(*constructor)(),
                               void(*copy)(void*, const void*)) {
            auto i = new _chan_st_(cap, destructor, constructor, copy);
            return i;
        }
    }

}
