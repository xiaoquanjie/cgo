/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/10
//----------------------------------------------------------------*/

#include "channel.h"
#include "common/circle_queue.h"
#include "common/slist.h"
#include "scheduler/cosignal.h"
#include <mutex>

#define M_GET_SELF(id) id = scheduler::cur_coid();
#define M_IN_CO(tid) (tid != (uint64_t)-1)

namespace cgo {
    namespace channel {
        struct recv_task {
            cgo::signal sig;
        };
        struct send_task {
            cgo::signal sig;
            const void* data;
        };

        using sendqueue = slist<send_task>;
        using recvqueue = slist<recv_task>;
        using bufqueue  = cqueue<void*>;

        struct _chan_st_ : public _i_chan_st_ {
            bool _closed = false;
            bufqueue _bufq;
            sendqueue _sendq;
            recvqueue _recvq;
            std::mutex _mu;

            void(*_free)(void*);
            void*(*_malloc)(const void*);
            void(*_copy)(void*, const void*);

            _chan_st_(int cap,
                      void(*free)(void*),
                      void*(*malloc)(const void*),
                      void(*copy)(void*, const void*))
                : _bufq(cap) {
                _free = free;
                _malloc = malloc;
                _copy = copy;
            }

            ~_chan_st_() {
                this->close();
                for (void* d; _bufq.pop(d);) {
                    _free(d);
                }
            }

            inline void _copyfree(void*& dst, void* src) {
                if (dst) {
                    _copy(dst, src);
                }
                _free(src);
            }

            bool recv(void* v) {
                _mu.lock();
                if (_closed) {
                    _mu.unlock();
                    return false;
                }

                // 看发送队列中有没有数据
                void* buf = 0;
                for (send_task task; _sendq.pop(task); ) {
                    // 从缓存中读出数据
                    if (_bufq.pop(buf)) {
                        // 将发送者数据入缓存
                        _bufq.push(_malloc(task.data));
                        _mu.unlock();
                        // 拷贝缓存里的数据
                        _copyfree(v, buf);
                    } else {
                        _mu.unlock();
                        // 直接拷贝发送者
                        _copy(v, task.data);
                    }

                    // 唤醒等待队列的协程
                    task.sig.post();
                    return true;
                }

                // 看缓存中有没有数据
                if (_bufq.pop(buf)) {
                    // 有数据
                    _mu.unlock();
                    // copy data
                    _copyfree(v, buf);
                    return true;
                }

                // 都没有数据，将自已入队列
                recv_task self;
                self.sig.init();
                _recvq.push(self);
                _mu.unlock();

                // 则将自己挂起来
                self.sig.wait(buf);
                // 关闭信号
                self.sig.close();

                // 恢复后
                if (_closed) {
                    assert(buf == 0);
                    return false;
                }

                // 拷贝数据
                assert(buf != 0);
                _copyfree(v, buf);
                return true;
            }

            bool send(const void* v) override {
                _mu.lock();
                if (_closed) {
                    _mu.unlock();
                    return false;
                }

                // 看接收队列有没有数据
                // 存在接受队列，则说明缓存是空的
                for (recv_task task; _recvq.pop(task);) {
                    // 有数据
                    _mu.unlock();
                    // 构造一条新数据
                    auto newv = _malloc(v);
                    // 唤醒等待协程
                    task.sig.post(newv);
                    return true;
                }

                // 看缓存是否满了
                if (!_bufq.full()) {
                    // 构造一条新数据
                    auto newv = _malloc(v);
                    _bufq.push(newv);
                    _mu.unlock();
                    return true;
                }

                // 将入自已入队列
                send_task self;
                self.sig.init();
                self.data = v;
                _sendq.push(self);
                _mu.unlock();

                // 将自己挂起来
                self.sig.wait();
                self.sig.close();

                if (_closed) {
                    return false;
                }
                return true;
            }

            void close() override {
                _mu.lock();
                if (_closed) {
                    _mu.unlock();
                    return;
                }
                _closed = true;
                _mu.unlock();

                for (recv_task task; _recvq.pop(task);) {
                    task.sig.post();
                }
                for (send_task task; _sendq.pop(task);) {
                    task.sig.post();
                }
            }
        };

        std::shared_ptr<_i_chan_st_> make_chan(int cap,
                                               void(*free)(void*),
                                               void*(*malloc)(const void*),
                                               void(*copy)(void*, const void*)) {
            auto i = std::make_shared<_chan_st_>(cap, free, malloc, copy);
            return i;
        }
    }

}
