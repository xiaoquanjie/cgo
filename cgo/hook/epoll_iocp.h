//
// Created by xiaoqj on 2024/2/21.
//

#pragma once

#include <stdio.h>
#include <atomic>
#include <assert.h>
#include <list>
#include <vector>
#include <mutex>
#include <thread>

#ifdef __GNUC__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/select.h>
#elif _MSC_VER
#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#endif

#define hook_debug(func) //_hook_debug(func)
inline void _hook_debug(const char* name) {
    printf("hook: %s ok\n", name);
}

#undef CGO_MAX_HOOK_FD
#define CGO_MAX_HOOK_FD 1024*100

#ifdef _MSC_VER
struct WinOverlapped {
    OVERLAPPED overlap;
    int err;
    DWORD bytes; // Number Of Bytes Sent / recv
};
struct AcceptOverlapped : public WinOverlapped {
    SOCKET l_fd;
    SOCKET c_fd;
    char addr_buf[128];
};
struct SendOverlapped : public WinOverlapped {
    WSABUF buf[1];
};
struct RecvOverlapped : public SendOverlapped {};
struct ConnOverlapped : public WinOverlapped {
    const struct sockaddr* name;
    int namelen;
};
struct SendtoOverlapped : public SendOverlapped {
    const struct sockaddr* name;
    int namelen;
};
struct RecvfromOverlapped : public RecvOverlapped {
    struct sockaddr* name;
    int* namelen;
};
#endif

namespace cgo {
    namespace hook {
        struct _epoll_iocp_st_;

        struct fd_state {
            enum {
                set = 1,
                read = 2,
                write = 4,
                accept = 8,
                connect = 16,
                connecok = 32,
            };

            volatile uint64_t co_id = -1;
            std::atomic_char flag = 0;
            _epoll_iocp_st_* volatile epoll_iocp = 0;
        };

        fd_state* get_fd_state(int fd);
        void clear_fd_state(int fd);
        fd_state* check_fd_state(int fd, int fs);
        bool canhook(int fd);
        bool canhook_poll_select();
        ///////////////////////////////////////////

        struct _epoll_iocp_st_ {
            std::atomic_int _fd_cnt = 0;
#ifdef __GNUC__
            int _epoll_fd = -1;
#elif _MSC_VER
            void* _iocp_handle = NULL;
#endif
            void init();

            void add_fd(int fd);

            void remove_fd(int fd);

            void setnonblock(int fd);

            void loop();

            int fd_count();
        };

        struct _epoll_iocp_mgr_ {
            std::vector<_epoll_iocp_st_*> _epoll_iocps;
            std::vector<std::thread*> _threads;
            std::mutex _mu;

            _epoll_iocp_mgr_();
            _epoll_iocp_st_* add_fd(int fd);
        };

        _epoll_iocp_mgr_* default_epoll_iocp_mgr();
    }
}
