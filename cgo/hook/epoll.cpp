//
// Created by xiaoqj on 2024/2/22.
//

#ifdef __GNUC__
#include "epoll_iocp.h"
#include "scheduler/scheduler.h"
#include <sys/socket.h>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/select.h>

namespace cgo {
    namespace hook {
        void _epoll_iocp_st_::init() {
            if (this->_epoll_fd != -1) {
                return;
            }

            this->_epoll_fd = epoll_create(1);
            if (this->_epoll_fd == -1) {
                throw "epoll_create error";
            }
        }

        void _epoll_iocp_st_::add_fd(int fd) {
            setnonblock(fd);
            this->_fd_cnt++;

            epoll_event event;
            event.data.fd = fd;
            event.events = EPOLLIN | EPOLLOUT | EPOLLERR;
            if (epoll_ctl(this->_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
                if (errno != EEXIST) {
                    throw "epoll add fd error";
                }
            }
        }

        void _epoll_iocp_st_::remove_fd(int fd) {
            epoll_ctl(this->_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            this->_fd_cnt--;
        }

        void _epoll_iocp_st_::setnonblock(int fd) {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags != -1 && !(flags & O_NONBLOCK)) {
                flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }

            if (flags == -1) {
                throw "fcntl nonblock fd error";
            }
        }

        void _epoll_iocp_st_::loop() {
            if (this->_fd_cnt <= 0) {
                return;
            }

            const int MAX_EVENTS = 500;
            epoll_event eventList[MAX_EVENTS];
            int fd_in_state[2] = {fd_state::read, fd_state::accept};
            int fd_out_state[3] = {fd_state::write, fd_state::accept, fd_state::connect};

            for (;;) {
                if (this->_fd_cnt <= 0) {
                    break;
                }

                for (int trys = 1; trys <= 10; trys++) {
                    int cnt = epoll_wait(this->_epoll_fd, eventList, MAX_EVENTS, 1000 * 10);
                    if (cnt < 0) {
                        if (errno != EINTR) {
                            throw "epoll_wait error";
                        }
                    } else if (cnt == 0) {
                        continue;
                    }

                    for (int i = 0; i < cnt; i++) {
                        if (eventList[i].events & EPOLLIN || eventList[i].events & EPOLLERR) {
                            auto state = get_fd_state(eventList[i].data.fd);
                            for (size_t idx = 0; idx < sizeof (fd_in_state) / sizeof (int); idx++) {
                                if (state->flag & fd_in_state[idx]) {
                                    state->flag ^= fd_in_state[idx];
                                    cgo::scheduler::schedule_post_signal(state->co_id, 0);
                                    break;
                                }
                            }
                        }

                        if (eventList[i].events & EPOLLOUT || eventList[i].events & EPOLLERR) {
                            int fd = eventList[i].data.fd;
                            auto state = get_fd_state(fd);
                            for (size_t idx = 0; idx < sizeof (fd_out_state) / sizeof (int); idx++) {
                                if (state->flag & fd_out_state[idx]) {
                                    state->flag ^= fd_out_state[idx];
                                    if (fd_out_state[idx] == fd_state::connect) {
                                        int error = 0;
                                        socklen_t len = sizeof(error);
                                        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0
                                            && error == 0) {
                                            state->flag |= fd_state::connecok;
                                        }
                                    }
                                    cgo::scheduler::schedule_post_signal(state->co_id, 0);
                                    break;
                                }
                            }
                        }
                    }
                }
            } // end for loop
        } // end for _epoll_iocp_st_
    } // end for namespace hook
} // end end for namespace cgo

#endif