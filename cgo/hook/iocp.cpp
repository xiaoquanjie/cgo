//
// Created by xiaoqj on 2024/2/22.
//

#ifdef _MSC_VER

#include "scheduler/scheduler.h"
#include "epoll_iocp.h"
#include <stdexcept>

namespace cgo::hook {
    void _epoll_iocp_st_::init() {
        if (this->_iocp_handle != nullptr) {
            return;
        }

        this->_iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (this->_iocp_handle == nullptr) {
            throw std::runtime_error("CreateIoCompletionPort error");
        }
    }

    void _epoll_iocp_st_::add_fd(int fd) {
        if (CreateIoCompletionPort((HANDLE)fd, this->_iocp_handle, (ULONG_PTR)fd, 0) == nullptr) {
            throw std::runtime_error("bind iocp error");
        }
        this->_fd_cnt++;
    }

    void _epoll_iocp_st_::remove_fd(int fd) {
        this->_fd_cnt--;
        // 不需要有什么与iocp解绑的动作
    }

    void _epoll_iocp_st_::modify_fd(int fd, int ctl) {
        // nothing to do
    }

    void _epoll_iocp_st_::setnonblock(int fd) {
    }

    void _epoll_iocp_st_::loop() {
        if (this->_fd_cnt <= 0) {
            return;
        }

        const int MAX_ENTRY = 200;
        OVERLAPPED_ENTRY completionPortEntries[MAX_ENTRY];
        ULONG ulNumEntriesRemoved = 0;

        for (;;) {
            if (this->_fd_cnt <= 0) {
                return;
            }

            BOOL ok = GetQueuedCompletionStatusEx(this->_iocp_handle,
                                                  completionPortEntries,
                                                  MAX_ENTRY,
                                                  &ulNumEntriesRemoved,
                                                  1000*10,
                                                  true);

            if (ok == FALSE) {
                continue;
            }

            for (ULONG idx = 0; idx < ulNumEntriesRemoved; idx++) {
                auto fd = completionPortEntries[idx].lpCompletionKey;
                auto state = cgo::hook::get_fd_state((int)fd);
                if (state->flag & state->connect) {
                    int optVal = -1;
                    int optLen = sizeof(optVal);
                    if (getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, (char*)&optVal, &optLen) == NO_ERROR) {
                        if (optVal != 0xFFFFFFFF) {
                            state->flag |= fd_state::connecok;
                        }
                    }
                }
                else {
                    auto overlapped = (WinOverlapped*)completionPortEntries[idx].lpOverlapped;
                    overlapped->bytes = completionPortEntries[idx].dwNumberOfBytesTransferred; // dwTrans is 0 when socket is closed or error but accept
                    overlapped->err = ERROR_SUCCESS;
                }
                cgo::scheduler::schedule_post_signal(state->co_id, nullptr);
            }
        } // end for loop
    } // end for _epoll_iocp_st_
} // end for namespace hook

#endif