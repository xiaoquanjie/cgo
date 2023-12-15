/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/10
//----------------------------------------------------------------*/

#include "channel.h"
#include "../coroutine/macro.h"
#include "../coroutine/coroutine.h"
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
            std::function<void(void*)> _destructor;

            std::mutex _mu;
            slist<_wait_st_> _sendq;
            slist<int64_t> _recvq;

            _chan_st_(int cap, const std::function<void(void*)>& destructor)
                : _buf(cap), _destructor(destructor) {
            }

            ~_chan_st_() {
                this->close();

                void* v = 0;
                while (_buf.pop(v)) {
                    _destructor(v);
                }
            }

            bool recv(void*& v) {
                auto co_id = coroutine::curid();
                if (co_id == M_INVALID_COROUTINE_ID) {
                    throw "not allow to read chan in non-coroutine";
                }

                this->_mu.lock();
                if (this->_closed) {
                    this->_mu.unlock();
                    return false;
                }

                if (this->_buf.empty()) {
                    _wait_st_ wait;
                    if (_sendq.pop(wait)) {
                        this->_mu.unlock();

                        scheduler::schedule_wait_co(wait._co_id, 0);
                        v = wait._data;
                        return true;
                    }

					this->_recvq.push(co_id);
					this->_mu.unlock();

                    scheduler::schedule_yield(v);
                    //assert(v != 0);
					if (_closed) {
						return false;
					}
					return true;
                }

                this->_buf.pop(v);
                this->_mu.unlock();
                return true;
            }

            bool send(void* v) override {
                auto co_id = coroutine::curid();
                if (co_id == M_INVALID_COROUTINE_ID) {
                    throw "not allow to write chan in non-coroutine";
                }

                this->_mu.lock();
                if (this->_closed) {
                    this->_mu.unlock();
                    return false;
                }

                if (_buf.empty()) {
                    int64_t wait;
                    if (_recvq.pop(wait)) {
                        this->_mu.unlock();

                        assert(v != 0);
                        scheduler::schedule_wait_co(wait, v);
                        return true;
                    }
                }

                if (_buf.full()) {
                    _sendq.push(_wait_st_{v, co_id});
                    this->_mu.unlock();

                    scheduler::schedule_yield();
                    if (_closed) {
                        return false;
                    }
                    return true;
                }

                _buf.push(v);
                this->_mu.unlock();
                return true;
            }

            void close() override {
                std::unique_lock<std::mutex> lock(this->_mu);
                if (this->_closed) {
                    return;
                }

                this->_closed = true;

                while (_recvq.size()) {
                    auto co_id = _recvq.front();
                    _recvq.pop();
                    scheduler::schedule_co(co_id, 0);
                }

                while (_sendq.size()) {
                    auto wait = _sendq.front();
                    _sendq.pop();
                    _destructor(wait._data);
                    scheduler::schedule_co(wait._co_id, 0);
                }
            }
        };

        _i_chan_st_* make_chan(int cap, const std::function<void(void*)>& destructor) {
            auto i = new _chan_st_(cap, destructor);
            return i;
        }
    }

}
