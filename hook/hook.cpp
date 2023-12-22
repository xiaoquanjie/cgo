//
// Created by xiaoqj on 2023/12/20.
//

#include "hook.h"

#ifdef CGO_USE_HOOK

#include "../scheduler/scheduler.h"
#include <stdio.h>
#include <atomic>

#define hook_debug //_hook_debug

inline void _hook_debug(const char* name) {
    printf("hook: %s ok\n", name);
}

inline bool canhook() {
    if (cgo::coro_adapter::cur_coid() == -1) {
        return false;
    }
    return true;
}

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
};

static fd_state g_fd_state[CGO_MAX_HOOK_FD];

inline fd_state* get_fd_state(int fd) {
    if (fd >= CGO_MAX_HOOK_FD) {
        throw "over max fd";
    }
    return &g_fd_state[fd];
}

inline void clear_fd_state(int fd) {
    if (fd >= CGO_MAX_HOOK_FD) {
        throw "over max fd";
    }

    g_fd_state[fd].flag = 0;
    g_fd_state[fd].co_id = -1;
}

#ifdef __GNUC__

#include <fcntl.h>
#include <sys/epoll.h>

static int g_epoll_fd = -1;

inline bool check_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return flags & O_NONBLOCK;
}

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1 && !(flags & O_NONBLOCK)) {
        flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    if (flags == -1) {
        throw "fcntl nonblock fd error";
    }
}

void add_fd(int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLOUT | EPOLLERR;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        if (errno != EEXIST) {
            throw "epoll add fd error";
        }
    }
}

void remove_fd(int fd) {
    epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

inline fd_state* check_set_fd(int fd, int fs) {
    if (!check_nonblock(fd)) {
        set_nonblock(fd);
    }

    auto state = get_fd_state(fd);
    if ((state->flag & fd_state::set) == 0) {
        state->flag |= fd_state::set;
        add_fd(fd);
    }

    assert((state->flag & fs) == 0);
    return state;
}

typedef int (*socket_hook_t)(int, int, int);
static socket_hook_t socket_hook = (socket_hook_t)dlsym(RTLD_NEXT,"socket");
int socket(int domain, int type, int protocol) {
    hook_debug(__FUNCTION__);
    return socket_hook(domain, type, protocol);
}

typedef int (*accept_hook_t)(int fd, struct sockaddr *addr, socklen_t *addrlen);
static accept_hook_t accept_hook = (accept_hook_t)dlsym(RTLD_NEXT,"accept");
int accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!canhook()) {
        return accept_hook(fd, addr, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::accept);

    int ret = accept_hook(fd, addr, addrlen);
    if (ret > 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::accept;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = accept_hook(fd, addr, addrlen);
            if (ret > 0) {
                break;
            } else {
                // if errno is ECONNABORTED, then try again
                if (errno != ECONNABORTED && EINTR != errno && EAGAIN != errno) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef int (*connect_hook_t)(int, const struct sockaddr*, socklen_t);
static connect_hook_t connect_hook = (connect_hook_t)dlsym(RTLD_NEXT,"connect");
int connect(int fd, const struct sockaddr *address, socklen_t addrlen) {
    if (!canhook()) {
        return connect_hook(fd, address, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::connect);

    int ret = connect_hook(fd, address, addrlen);
    if (ret == 0) {
        return ret;
    }

    if (EINPROGRESS == errno
        || EAGAIN == errno
        || EINTR == errno) {
        for (;;) {
            state->flag |= fd_state::connect;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            if (state->flag & fd_state::connecok) {
                state->flag ^= fd_state::connecok;
                ret = 0;
                errno = 0;
            } else {
                errno = ECONNREFUSED;
            }
            break;
        }
    }

    return ret;
}

typedef int (*close_hook_t)(int fd);
static close_hook_t close_hook = (close_hook_t)dlsym(RTLD_NEXT,"close");
int close(int fd) {
    hook_debug(__FUNCTION__);
    auto state = get_fd_state(fd);
    if (state->flag & fd_state::set) {
        clear_fd_state(fd);
        remove_fd(fd);
    }

    return close_hook(fd);
}

typedef ssize_t (*read_hook_t)(int fildes, void *buf, size_t);
static read_hook_t read_hook = (read_hook_t)dlsym(RTLD_NEXT,"read");
ssize_t read(int fd, void *buf, size_t bytes) {
    if (!canhook()) {
        return read_hook(fd, buf, bytes);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::read);

    ssize_t ret = read_hook(fd, buf, bytes);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::read;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = read_hook(fd, buf, bytes);
            if (ret >= 0) {
                break;
            } else {
                // if errno is EINTR, then try again
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef ssize_t (*recv_hook_t) (int, void *__buff, size_t __len, int __flags);
static recv_hook_t recv_hook = (recv_hook_t)dlsym(RTLD_NEXT,"recv");
ssize_t recv(int fd, void *buf, size_t len, int flags) {
    if (!canhook()) {
        return recv_hook(fd, buf, len, flags);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::read);

    ssize_t ret = recv_hook(fd, buf, len, flags);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::read;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = recv_hook(fd, buf, len, flags);
            if (ret >= 0) {
                break;
            } else {
                // if errno is EINTR, then try again
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef ssize_t (*send_hook_t)(int, const void *__buff, size_t __len, int __flags);
static send_hook_t send_hook = (send_hook_t)dlsym(RTLD_NEXT,"send");
ssize_t send(int fd, const void *buf, size_t len, int flags) {
    if (!canhook()) {
        return send_hook(fd, buf, len, flags);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::write);

    ssize_t ret = send_hook(fd, buf, len, flags);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::write;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = send_hook(fd, buf, len, flags);
            if (ret >= 0) {
                break;
            } else {
                // if errno is EINTR, then try again
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef ssize_t (*write_hook_t)(int, const void*, size_t);
static write_hook_t write_hook = (write_hook_t)dlsym(RTLD_NEXT,"write");
ssize_t write(int fd, const void *buf, size_t bytes) {
    if (!canhook()) {
        return write_hook(fd, buf, bytes);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::write);

    ssize_t ret = write_hook(fd, buf, bytes);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::write;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = write_hook(fd, buf, bytes);
            if (ret >= 0) {
                break;
            } else {
                // if errno is EINTR, then try again
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef ssize_t (*sendto_hook_t)(int fd, const void *, size_t, int, const struct sockaddr*, socklen_t);
static sendto_hook_t sendto_hook = (sendto_hook_t)dlsym(RTLD_NEXT,"sendto");
ssize_t sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dstaddr, socklen_t addrlen) {
    if (!canhook()) {
        return sendto_hook(fd, buf, len, flags, dstaddr, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::write);

    ssize_t ret = sendto_hook(fd, buf, len, flags, dstaddr, addrlen);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::write;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = sendto_hook(fd, buf, len, flags, dstaddr, addrlen);
            if (ret >= 0) {
                break;
            } else {
                // if errno is EINTR, then try again
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef ssize_t (*recvfrom_hook_t)(int fd, void *buf, size_t len, int flags, struct sockaddr *srcaddr, socklen_t *addrlen);
static recvfrom_hook_t recvfrom_hook = (recvfrom_hook_t)dlsym(RTLD_NEXT,"recvfrom");
ssize_t recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *srcaddr, socklen_t *addrlen) {
    if (!canhook()) {
        return recvfrom_hook(fd, buf, len, flags, srcaddr, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = check_set_fd(fd, fd_state::read);

    ssize_t ret = recvfrom_hook(fd, buf, len, flags, srcaddr, addrlen);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            state->flag |= fd_state::read;
            state->co_id = cgo::coro_adapter::cur_coid();
            cgo::scheduler::schedule_yield();
            ret = recvfrom_hook(fd, buf, len, flags, srcaddr, addrlen);
            if (ret >= 0) {
                break;
            } else {
                // if errno is EINTR, then try again
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return ret;
}

typedef struct hostent* (*gethostbyname_hook_t)(const char *name);
static gethostbyname_hook_t gethostbyname_hook = (gethostbyname_hook_t)dlsym(RTLD_NEXT,"gethostbyname");
struct hostent* gethostbyname(const char *name) {
    hook_debug(__FUNCTION__);
    return gethostbyname_hook(name);
}

typedef unsigned int(*sleep_hook_t)(unsigned int seconds);
static sleep_hook_t sleep_hook = (sleep_hook_t)dlsym(RTLD_NEXT,"sleep");
unsigned int sleep(unsigned int seconds) {
    if (!canhook()) {
        return sleep_hook(seconds);
    }

    hook_debug(__FUNCTION__);
    cgo::scheduler::schedule_wait(seconds*1000);
    return 0;
}

typedef int (*usleep_hook_t)(useconds_t usec);
static usleep_hook_t usleep_hook = (usleep_hook_t)dlsym(RTLD_NEXT, "usleep");
int usleep(useconds_t microseconds) {
    if (!canhook()) {
        return usleep_hook(microseconds);
    }

    hook_debug(__FUNCTION__);
    cgo::scheduler::schedule_wait(microseconds/1000);
    return 0;
}

void linux_hook_init() {
    if (g_epoll_fd != -1) {
        return;
    }

    g_epoll_fd = epoll_create(1);
    if (g_epoll_fd == -1) {
        throw "epoll_create error";
    }

    cgo::scheduler::cgo_add_loop([]() {
        static const int MAX_EVENTS = 500;
        static epoll_event eventList[MAX_EVENTS];
        static int fd_in_state[2] = {fd_state::read, fd_state::accept};
        static int fd_out_state[3] = {fd_state::write, fd_state::accept, fd_state::connect};

        int cnt = epoll_wait(g_epoll_fd, eventList, MAX_EVENTS, 0);
        if (cnt < 0) {
            throw "epoll_wait error";
        } else if (cnt == 0) {
            return;
        }

        for (int i = 0; i < cnt; i++) {
            if (eventList[i].events & EPOLLIN || eventList[i].events & EPOLLERR) {
                auto state = get_fd_state(eventList[i].data.fd);
                for (int idx = 0; idx < sizeof (fd_in_state) / sizeof (int); idx++) {
                    if (state->flag & fd_in_state[idx]) {
                        state->flag ^= fd_in_state[idx];
                        cgo::scheduler::schedule_co(state->co_id, 0);
                        break;
                    }
                }
            }

            if (eventList[i].events & EPOLLOUT || eventList[i].events & EPOLLERR) {
                int fd = eventList[i].data.fd;
                auto state = get_fd_state(fd);
                for (int idx = 0; idx < sizeof (fd_out_state) / sizeof (int); idx++) {
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
                        cgo::scheduler::schedule_co(state->co_id, 0);
                        break;
                    }
                }
            }
        }
    });
}

#elif _MSC_VER

#define WIN32_LEAN_AND_MEAN
#define MIN_HOOK_IMPLEMENTATION
#include <WinSock2.h>
#include "minhook.h"

#pragma comment(lib, "ws2_32.lib")

typedef SOCKET (PASCAL FAR *socket_hook_t)(_In_ int af,
    _In_ int type,
    _In_ int protocol);
static socket_hook_t socket_hook = 0;
SOCKET PASCAL FAR hook_socket(
    _In_ int af,
    _In_ int type,
    _In_ int protocol) {
    hook_debug(__FUNCTION__);
    return socket_hook(af, type, protocol);
}

typedef SOCKET(PASCAL FAR *accept_hook_t)(
    _In_ SOCKET s,
    _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR* addr,
    _Inout_opt_ int FAR* addrlen);
static accept_hook_t accept_hook = 0;
SOCKET PASCAL FAR hook_accept (
                          _In_ SOCKET s,
                          _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
                          _Inout_opt_ int FAR *addrlen) {
    hook_debug(__FUNCTION__);
    return accept_hook(s, addr, addrlen);
}

typedef int (PASCAL FAR *connect_hook_t)(
    _In_ SOCKET s,
    _In_reads_bytes_(namelen) const struct sockaddr FAR* name,
    _In_ int namelen);
static connect_hook_t connect_hook = 0;
int PASCAL FAR hook_connect (
                        _In_ SOCKET s,
                        _In_reads_bytes_(namelen) const struct sockaddr FAR *name,
                        _In_ int namelen) {
    return 0;
}

typedef int (PASCAL FAR* closesocket_hook_t)(IN SOCKET s);
static closesocket_hook_t closesocket_hook = 0;
int PASCAL FAR hook_closesocket (IN SOCKET s) {
    return 0;
}

typedef int (PASCAL FAR* sendto_hook_t)(
    _In_ SOCKET s,
    _In_reads_bytes_(len) const char FAR* buf,
    _In_ int len,
    _In_ int flags,
    _In_reads_bytes_opt_(tolen) const struct sockaddr FAR* to,
    _In_ int tolen);
static sendto_hook_t sendto_hook = 0;
int PASCAL FAR hook_sendto (
                       _In_ SOCKET s,
                       _In_reads_bytes_(len) const char FAR * buf,
                       _In_ int len,
                       _In_ int flags,
                       _In_reads_bytes_opt_(tolen) const struct sockaddr FAR *to,
                       _In_ int tolen) {
    return 0;
}

typedef int (PASCAL FAR* recvfrom_hook_t)(
    _In_ SOCKET s,
    _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR* buf,
    _In_ int len,
    _In_ int flags,
    _Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR* from,
    _Inout_opt_ int FAR* fromlen);
static recvfrom_hook_t recvfrom_hook = 0;
int PASCAL FAR hook_recvfrom (
                         _In_ SOCKET s,
                         _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
                         _In_ int len,
                         _In_ int flags,
                         _Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR * from,
                         _Inout_opt_ int FAR * fromlen) {
    return 0;
}

typedef int (PASCAL FAR* send_hook_t)(
    _In_ SOCKET s,
    _In_reads_bytes_(len) const char FAR* buf,
    _In_ int len,
    _In_ int flags);
static send_hook_t send_hook = 0;
int PASCAL FAR hook_send (
                     _In_ SOCKET s,
                     _In_reads_bytes_(len) const char FAR * buf,
                     _In_ int len,
                     _In_ int flags) {
    return 0;
}

typedef int (PASCAL FAR* recv_hook_t)(
    _In_ SOCKET s,
    _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR* buf,
    _In_ int len,
    _In_ int flags);
static recv_hook_t recv_hook = 0;
int PASCAL FAR hook_recv (
                     _In_ SOCKET s,
                     _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
                     _In_ int len,
                     _In_ int flags) {
    return 0;
}

typedef struct hostent FAR* (PASCAL FAR* gethostbyname_hook_t)(_In_z_ const char FAR* name);
static gethostbyname_hook_t gethostbyname_hook = 0;
struct hostent FAR * PASCAL FAR hook_gethostbyname(_In_z_ const char FAR * name) {
    return 0;
}

typedef void (WINAPI *sleep_hook_t)(_In_ DWORD dwMilliseconds);
static sleep_hook_t sleep_hook = 0;
void WINAPI hook_sleep(_In_ DWORD dwMilliseconds) {
    if (!canhook()) {
        sleep_hook(dwMilliseconds);
    }
    else {
        hook_debug(__FUNCTION__);
        cgo::scheduler::schedule_wait(dwMilliseconds);
    }
}

#define HOOK_API(module, oriname, name) \
if (MH_CreateHookApi(TEXT(module), #oriname, &hook_##name, reinterpret_cast<LPVOID*>(&name##_hook)) != MH_OK) { \
    throw "hook " #oriname " error"; \
} \
if (MH_EnableHook(&oriname) != MH_OK) {\
    throw "enable hook " #oriname " error"; \
}

#define HOOK_API2(oriname, name) \
if (MH_CreateHook(&oriname, &hook_##name, reinterpret_cast<LPVOID*>(&name##_hook)) != MH_OK) { \
    throw "hook " #oriname " error"; \
} \
if (MH_EnableHook(&oriname) != MH_OK) {\
    throw "enable hook " #oriname " error"; \
}

bool win_hook_init() {
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        throw "wsa startup error";
    }

    if (MH_Initialize() != MH_OK) {
        throw "init minhook error";
    }

    HOOK_API("kernel32.dll", Sleep, sleep);
    HOOK_API2(socket, socket);
    HOOK_API2(accept, accept);
    HOOK_API2(connect, connect);
    HOOK_API2(closesocket, closesocket);
    HOOK_API2(sendto, sendto);
    HOOK_API2(recvfrom, recvfrom);
    HOOK_API2(send, send);
    HOOK_API2(recv, recv);
    //HOOK_API2(gethostbyname, gethostbyname);
    return true;
}

#endif

struct hook_init {
    hook_init() {
#ifdef _MSC_VER
        win_hook_init();
#elif __GNUC__
        linux_hook_init();
#endif
    }
};
static hook_init shookinit;

namespace cgo {
    namespace hook {
        void mSleep(unsigned int millisecond) {
#ifdef __GNUC__
            usleep((useconds_t)(millisecond*1000));
#elif _MSC_VER
            Sleep(millisecond);
#else
#pragma message("no msleep implement")
#endif
        }

        void enable_hook() {

        }
    }
}

#endif