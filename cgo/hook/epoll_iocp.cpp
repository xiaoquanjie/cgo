//
// Created by xiaoqj on 2024/2/21.
//

#include "epoll_iocp.h"
#include "scheduler/scheduler.h"
#include <stdexcept>
#include <string>

namespace cgo::hook {
    fd_state g_fd_state[CGO_MAX_HOOK_FD];
    bool g_global_hook = false;

    fd_state* get_fd_state(int fd) {
        if (fd >= CGO_MAX_HOOK_FD) {
            throw std::runtime_error(std::string("over max fd:") + std::to_string(fd));
        }
        return &g_fd_state[fd];
    }

    void clear_fd_state(int fd) {
        if (fd >= CGO_MAX_HOOK_FD) {
            throw std::runtime_error(std::string("over max fd:") + std::to_string(fd));
        }

        g_fd_state[fd].flag = 0;
        g_fd_state[fd].co_id = -1;
        g_fd_state[fd].epoll_iocp->remove_fd(fd);
        g_fd_state[fd].epoll_iocp = nullptr;
    }

    fd_state* check_fd_state(int fd, int fs) {
        auto state = get_fd_state(fd);
        if (state->flag & fs) {
            throw std::runtime_error(std::string("duplicate state fd:") + std::to_string(fd));
        }
        assert((state->flag & fs) == 0);
        return state;
    }

    // hook a fd
    void hook_fd(int fd) {
        if (fd == 0) {
            return;
        }
        auto state = get_fd_state(fd);
        if ((state->flag & fd_state::set) == 1) {
            return;
        }

        state->flag |= fd_state::set;
        auto st = default_epoll_iocp_mgr()->add_fd(fd);
        state->epoll_iocp = st;
    }

    bool canhook(int fd) {
        if (cgo::scheduler::cur_coid() == (uint64_t)-1) {
            return false;
        }

        auto state = get_fd_state(fd);
        if ((state->flag & fd_state::set) == 1) {
            return true;
        } else {
            if (g_global_hook) {
                hook_fd(fd);
                return true;
            }
        }
        return false;
    }

    bool canhook_poll_select() {
        if (cgo::scheduler::cur_coid() == (uint64_t)-1) {
            return false;
        }
        return (g_global_hook || cgo::scheduler::co_hook());
    }

    // default disable global hook
    void global_hook(bool hook) {
        g_global_hook = hook;
    }

    void hook_poll_select(bool hook) {
        scheduler::co_hook(hook);
    }

    //////////////////////////////////////////////

    int _epoll_iocp_st_::fd_count() {
        return this->_fd_cnt.load();
    }

    _epoll_iocp_mgr_::_epoll_iocp_mgr_() {
        auto concur = std::thread::hardware_concurrency();
        for (unsigned int idx = 0; idx < concur; idx++) {
            auto st = new _epoll_iocp_st_;
            st->init();
            this->_epoll_iocps.push_back(st);
            this->_threads.push_back(nullptr);
        }
    }

    _epoll_iocp_st_* _epoll_iocp_mgr_::add_fd(int fd) {
        std::unique_lock<std::mutex> ul(this->_mu);
        _epoll_iocp_st_* st = nullptr;
        size_t idx = 0;
        for (idx = 0; idx < this->_epoll_iocps.size(); idx++) {
            st = this->_epoll_iocps[idx];
            if (st->fd_count() < 100) {
                goto add;
            }
        }

        idx = fd % (int)this->_epoll_iocps.size();
        st = this->_epoll_iocps[idx];
        add:
        st->add_fd(fd);
        if (this->_threads[idx] == 0) {
            this->_threads[idx] = new std::thread([this, st, idx] {
                //printf("[cgo debug] start new hook thread\n");
                st->loop();
                this->_mu.lock();
                this->_threads[idx] = 0;
                this->_mu.unlock();
                //printf("[cgo debug] quit hook thread\n");
            });
            this->_threads[idx]->detach();
        }
        return st;
    }

    _epoll_iocp_mgr_* default_epoll_iocp_mgr() {
        static _epoll_iocp_mgr_ mgr;
        return &mgr;
    }
}