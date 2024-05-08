//
// Created by xiaoqj on 2024/4/29.
//

#include "timer.h"
#include <stdexcept>
#include <cstring>
#include <cassert>

namespace timer {
    const uint16_t Timer::TimerInfo::_sWheelLen;
    const uint8_t Timer::TimerInfo::_sWheelLevel;

    uint64_t allocTimeId(Timer::TimerInfo* info, uint16_t pos, uint8_t level) {
        uint64_t id = info->_id++;
        if (info->_id == 0xFFFFFFFE) {
            info->_id = 1;
        }
        id <<= 32;
        id += (uint32_t)pos << 16;
        id += level;
        return id;
    }

    void decodeTimeId(uint64_t id, uint16_t *pos, uint8_t *level) {
        uint32_t d = id & 0xFFFFFFFF;
        *pos = d >> 16;
        *level = d & 0xFFFF;
    }

    bool calcPosLevel(Timer::TimerInfo *info, uint64_t now, uint32_t interval, uint16_t *pos, uint8_t *level) {
        auto future = now + interval;
        auto diff = future - info->_now;
        if (diff >= Timer::TimerInfo::_sWheelLevel * Timer::TimerInfo::_sWheelLen) {
            return false;
        }

        diff = future - info->_beg;
        auto count = diff;
        *pos = count % Timer::TimerInfo::_sWheelLen;
        *level = count / Timer::TimerInfo::_sWheelLen;
        if (*level >= Timer::TimerInfo::_sWheelLevel) {
            throw std::runtime_error("[cgo timer] over timer wheel level");
        }
        return true;
    }

    Timer::tnode_list* getNodeList(Timer::TimerInfo *info, uint16_t pos, uint8_t level) {
        if (pos >= Timer::TimerInfo::_sWheelLen || level >= Timer::TimerInfo::_sWheelLevel) {
            throw std::runtime_error("[cgo timer] invalid pos or level");
        }

        auto nlArr = info->_wheel[pos];
        if (!nlArr) {
            nlArr = info->_wheel[pos] = new Timer::tnode_list*[Timer::TimerInfo::_sWheelLevel] {nullptr};
            std::fill(info->_wheel[pos], info->_wheel[pos] + Timer::TimerInfo::_sWheelLevel, nullptr);
        }
        auto nl = nlArr[level];
        if (!nl) {
            nl = nlArr[level] = new Timer::tnode_list;
        }
        return nl;
    }

    void update(Timer::TimerInfo *info) {
        auto last = info->_now;
        info->_now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto span = info->_now - last;
        for (uint64_t idx = 0; idx <= span; idx++) {
            auto diff = last - info->_beg + idx;
            uint16_t pos = diff % Timer::TimerInfo::_sWheelLen;
            uint8_t level = diff / Timer::TimerInfo::_sWheelLen;

            auto nlArr = info->_wheel[pos];
            if (!nlArr) continue;
            auto nl = nlArr[level];
            if (!nl) continue;
            for (auto iter = nl->begin(); iter != nl->end(); iter++) {
                info->_notify(iter->id, iter->payload);
            }
            info->_count -= nl->size();
            delete nl;
            nlArr[level] = nullptr;
        }
    }

    Timer::Timer(const std::function<void(uint64_t, uint64_t)>& notify) {
        _info._notify = notify;
        _info._now = _info._beg = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        std::fill(_info._wheel, _info._wheel + TimerInfo::_sWheelLen, nullptr);
    }

    Timer::~Timer() {
        for (auto nlArr : _info._wheel) {
            if (!nlArr) {
                continue;
            }
            // nl分了_sWheelLevel层
            for (int j = 0; j < TimerInfo::_sWheelLevel; j++) {
                auto nl = nlArr[j];
                delete nl;
            }
            delete []nlArr;
        }
    }

    uint32_t Timer::count() const {
        return _info._count;
    }

    uint64_t Timer::add(uint32_t interval, uint64_t payload) {
        if (interval == 0) {
            return false;
        }
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        uint16_t pos = 0;
        uint8_t level = 0;
        if (!calcPosLevel(&_info, now, interval, &pos, &level)) {
            return 0;
        }
        auto nl = getNodeList(&_info, pos, level);
        auto id = allocTimeId(&_info, pos, level);
        nl->push_back(tnode{id, payload});
        _info._count++;
        return id;
    }

    void Timer::update() {
        timer::update(&_info);
    }

    //////////////////////////////////////////////////

    SafeTimer::SafeTimer(const std::function<void(uint64_t, uint64_t)>& notify) {
        _info._notify = notify;
        _info._now = _info._beg = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        std::fill(_info._wheel, _info._wheel + Timer::TimerInfo::_sWheelLen, nullptr);
    }

    uint32_t SafeTimer::count() const {
        return _info._count;
    }

    uint64_t SafeTimer::add(uint32_t interval, uint64_t payload) {
        if (interval == 0) {
            return false;
        }
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        uint16_t pos = 0;
        uint8_t level = 0;
        if (!calcPosLevel(&_info, now, interval, &pos, &level)) {
            return 0;
        }

        auto id = allocTimeId(&_info, pos, level);
        uint64_t expire = interval + now;
        safenode node = {id, payload, expire};
        _waits.enqueue(std::move(node));
        _info._count++;
        return id;
    }

    void SafeTimer::update() {
        for (int i = 0; i < 10000; i++) {
            safenode node;
            if (!_waits.try_dequeue(node)) {
                break;
            }

            // 判断该节点是否超时了
            if (_info._now >= node.expire) {
                _info._notify(node.id, node.payload);
                _info._count--;
            } else {
                uint16_t pos = 0;
                uint8_t level = 0;
                decodeTimeId(node.id, &pos, &level);
                auto nl = getNodeList(&_info, pos, level);
                nl->push_back(Timer::tnode{node.id, node.payload});
            }
        }
        timer::update(&_info);
    }

    /////////////////////////////////////////////////////////

    Timer* Timer::NewTimer(const std::function<void(uint64_t, uint64_t)>& notify) {
        auto t = new Timer(notify);
        return t;
    }

    Timer* NewTimer(const std::function<void(uint64_t, uint64_t)>& notify) {
        return Timer::NewTimer(notify);
    }

    SafeTimer* SafeTimer::NewSafeTimer(const std::function<void(uint64_t, uint64_t)>& notify) {
        return new SafeTimer(notify);
    }

    SafeTimer* NewSafeTimer(const std::function<void(uint64_t, uint64_t)>& notify) {
        return SafeTimer::NewSafeTimer(notify);
    }
}