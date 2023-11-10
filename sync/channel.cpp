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
#include "../common/slist.h"
#include <mutex>
#include <condition_variable>

namespace cgo {
    namespace channel {
        struct _chan_st_ : public _i_chan_st_ {
            int _cap = 0;
            int _size = 0;
            std::shared_ptr<void>* _data;

            std::mutex _mu;
            std::condition_variable _cond;
            slist<int64_t> _wait;

            ~_chan_st_() {
                delete _data;
            }

            bool read(std::shared_ptr<void>& v) {
                auto co_id = coroutine::curid();
                if (co_id == M_INVALID_COROUTINE_ID) {
                    assert(0==1 && ("not allow to op chan in non-coroutine"));
                    return false;
                }


                return false;
            }

            bool write(std::shared_ptr<void>& v) override {
                auto co_id = coroutine::curid();
                if (co_id == M_INVALID_COROUTINE_ID) {
                    assert(0==1 && ("not allow to op chan in non-coroutine"));
                    return false;
                }
                return false;
            }

            bool empty() {
                return false;
            }

            bool full() {
                return false;
            }
        };

        std::shared_ptr<_i_chan_st_> make_chan(int cap) {
            auto i = std::make_shared<_chan_st_>();
            if (cap > 0) {
                i->_cap = cap;
                i->_data = new std::shared_ptr<void>[cap];
            }
            return i;
        }
    }

}
