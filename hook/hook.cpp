//
// Created by xiaoqj on 2023/12/20.
//

#include "hook.h"

#ifdef CGO_USE_HOOK

#include "../scheduler/scheduler.h"
#include <stdio.h>

static void hook_debug(const char* name) {
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
        read_state = 1,
        write_state = 2,
    };

    volatile int fd = -1;
    volatile int flag = 0;
    volatile uint64_t co_id = 0;
};

static const int g_max_fd = 4*1024*100;
static fd_state g_fd_state[g_max_fd];

inline fd_state* get_fd_state(int fd) {
    if (fd >= g_max_fd) {
        throw "over max fd";
    }
    return &g_fd_state[fd];
}

inline void clear_fd_state(int fd) {
    if (fd >= g_max_fd) {
        throw "over max fd";
    }

    g_fd_state[fd].flag = 0;
    g_fd_state[fd].fd = -1;
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
        throw "epoll add fd error";
    }
}

void remove_fd(int fd) {
    epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

typedef int (*socket_hook_t)(int, int, int);
static socket_hook_t socket_hook = (socket_hook_t)dlsym(RTLD_NEXT,"socket");
int socket(int domain, int type, int protocol) {
    return socket_hook(domain, type, protocol);
}

typedef int (*accept_hook_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
static accept_hook_t accept_hook = (accept_hook_t)dlsym(RTLD_NEXT,"accept");
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return 0;
}

typedef int (*connect_hook_t)(int, const struct sockaddr*, socklen_t);
static connect_hook_t connect_hook = (connect_hook_t)dlsym(RTLD_NEXT,"connect");
int connect(int fd, const struct sockaddr *address, socklen_t addrlen) {
    return 0;
}

typedef int (*close_hook_t)(int fd);
static close_hook_t close_hook = (close_hook_t)dlsym(RTLD_NEXT,"close");
int close(int fd) {
    auto state = get_fd_state(fd);
    if (state->fd != -1) {
        clear_fd_state(fd);
    }

    return close_hook(fd);
}

typedef ssize_t (*read_hook_t)(int fildes, void *buf, size_t);
static read_hook_t read_hook = (read_hook_t)dlsym(RTLD_NEXT,"read");
ssize_t read(int fd, void *buf, size_t bytes) {
    if (!canhook()) {
        return read_hook(fd, buf, bytes);
    }

    if (check_nonblock(fd)) {
        set_nonblock(fd);
        add_fd(fd);
        auto state = get_fd_state(fd);
        state->fd = fd;
    }

    auto state = get_fd_state(fd);
    assert((state->flag & fd_state::read_state) == 0);
    state->flag |= fd_state::read_state;
    state->co_id = cgo::coro_adapter::cur_coid();

    cgo::scheduler::schedule_yield();
    return read_hook(fd, buf, bytes);
}

typedef ssize_t (*write_hook_t)(int, const void*, size_t);
static write_hook_t write_hook = (write_hook_t)dlsym(RTLD_NEXT,"write");
ssize_t write(int fd, const void *buf, size_t bytes) {
    if (!canhook()) {
        return write_hook(fd, buf, bytes);
    }

    if (check_nonblock(fd)) {
        set_nonblock(fd);
        add_fd(fd);
        auto state = get_fd_state(fd);
        state->fd = fd;
    }

    return 0;
}

typedef ssize_t (*sendto_hook_t)(int socket, const void *, size_t, int, const struct sockaddr*, socklen_t);
static sendto_hook_t sendto_hook = (sendto_hook_t)dlsym(RTLD_NEXT,"sendto");
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    return 0;
}

typedef ssize_t (*recvfrom_hook_t)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
static recvfrom_hook_t recvfrom_hook = (recvfrom_hook_t)dlsym(RTLD_NEXT,"recvfrom");
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return 0;
}

typedef struct hostent* (*gethostbyname_hook_t)(const char *name);
static gethostbyname_hook_t gethostbyname_hook = (gethostbyname_hook_t)dlsym(RTLD_NEXT,"gethostbyname");
struct hostent* gethostbyname(const char *name) {
    return 0;
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

        int cnt = epoll_wait(g_epoll_fd, eventList, MAX_EVENTS, 0);
        if (cnt < 0) {
            throw "epoll_wait error";
        } else if (cnt == 0) {
            return;
        }

        for (int i = 0; i < cnt; i++) {
            if (eventList[i].events & EPOLLIN || eventList[i].events & EPOLLERR) {
                auto state = get_fd_state(eventList[i].data.fd);
                if (state->flag & fd_state::read_state) {
                    state->flag ^= fd_state::read_state;
                    cgo::scheduler::schedule_co(state->co_id, 0);
                }
            }

            if (eventList[i].events & EPOLLOUT || eventList[i].events & EPOLLERR) {
                auto state = get_fd_state(eventList[i].data.fd);
                if (state->flag & fd_state::write_state) {
                    state->flag ^= fd_state::write_state;
                    cgo::scheduler::schedule_co(state->co_id, 0);
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
    return socket_hook(af, type, protocol);
}

SOCKET PASCAL FAR accept (
                          _In_ SOCKET s,
                          _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
                          _Inout_opt_ int FAR *addrlen) {
    return 0;
}

int PASCAL FAR connect (
                        _In_ SOCKET s,
                        _In_reads_bytes_(namelen) const struct sockaddr FAR *name,
                        _In_ int namelen) {
    return 0;
}

int PASCAL FAR closesocket (IN SOCKET s) {
    return 0;
}

int PASCAL FAR sendto (
                       _In_ SOCKET s,
                       _In_reads_bytes_(len) const char FAR * buf,
                       _In_ int len,
                       _In_ int flags,
                       _In_reads_bytes_opt_(tolen) const struct sockaddr FAR *to,
                       _In_ int tolen) {
    return 0;
}

int PASCAL FAR recvfrom (
                         _In_ SOCKET s,
                         _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
                         _In_ int len,
                         _In_ int flags,
                         _Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR * from,
                         _Inout_opt_ int FAR * fromlen) {
    return 0;
}

int PASCAL FAR send (
                     _In_ SOCKET s,
                     _In_reads_bytes_(len) const char FAR * buf,
                     _In_ int len,
                     _In_ int flags) {
    return 0;
}

int PASCAL FAR recv (
                     _In_ SOCKET s,
                     _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
                     _In_ int len,
                     _In_ int flags) {
    return 0;
}


struct hostent FAR * PASCAL FAR gethostbyname(_In_z_ const char FAR * name) {
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