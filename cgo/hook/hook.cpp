//
// Created by xiaoqj on 2023/12/20.
//

#include "hook.h"
#include "epoll_iocp.h"
#include "scheduler/scheduler.h"

#define M_SET_STATE(data, state) data->co_id = cgo::scheduler::cur_coid(); data->flag |= cgo::hook::state;

#ifdef __GNUC__

typedef int (*socket_hook_t)(int, int, int);
static socket_hook_t socket_hook = (socket_hook_t)dlsym(RTLD_NEXT,"socket");
int socket(int domain, int type, int protocol) {
    hook_debug(__FUNCTION__);
    return socket_hook(domain, type, protocol);
}

typedef int (*accept_hook_t)(int fd, struct sockaddr *addr, socklen_t *addrlen);
static accept_hook_t accept_hook = (accept_hook_t)dlsym(RTLD_NEXT,"accept");
int accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!cgo::hook::canhook(fd)) {
        return accept_hook(fd, addr, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::accept);

    int ret = accept_hook(fd, addr, addrlen);
    if (ret > 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::accept);
            cgo::scheduler::schedule_wait_signal();
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
    if (!cgo::hook::canhook(fd)) {
        return connect_hook(fd, address, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::connect);

    int ret = connect_hook(fd, address, addrlen);
    if (ret == 0) {
        return ret;
    }

    if (EINPROGRESS == errno
        || EAGAIN == errno
        || EINTR == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::connect);
            cgo::scheduler::schedule_wait_signal();
            if (state->flag & cgo::hook::fd_state::connecok) {
                state->flag ^= cgo::hook::fd_state::connecok;
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
    auto state = cgo::hook::get_fd_state(fd);
    if (state->flag & cgo::hook::fd_state::set) {
        hook_debug(__FUNCTION__);
        cgo::hook::clear_fd_state(fd);
    }
    return close_hook(fd);
}

typedef ssize_t (*read_hook_t)(int fildes, void *buf, size_t);
static read_hook_t read_hook = (read_hook_t)dlsym(RTLD_NEXT,"read");
ssize_t read(int fd, void *buf, size_t bytes) {
    if (!cgo::hook::canhook(fd)) {
        return read_hook(fd, buf, bytes);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::read);

    ssize_t ret = read_hook(fd, buf, bytes);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            //hook_debug("read wait");
            M_SET_STATE(state, fd_state::read);
            cgo::scheduler::schedule_wait_signal();
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
    if (!cgo::hook::canhook(fd)) {
        return recv_hook(fd, buf, len, flags);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::read);

    ssize_t ret = recv_hook(fd, buf, len, flags);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::read);
            cgo::scheduler::schedule_wait_signal();
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
    if (!cgo::hook::canhook(fd)) {
        return send_hook(fd, buf, len, flags);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::write);

    ssize_t ret = send_hook(fd, buf, len, flags);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::write);
            cgo::scheduler::schedule_wait_signal();
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
    if (!cgo::hook::canhook(fd)) {
        return write_hook(fd, buf, bytes);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::write);

    ssize_t ret = write_hook(fd, buf, bytes);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::write);
            cgo::scheduler::schedule_wait_signal();
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
    if (!cgo::hook::canhook(fd)) {
        return sendto_hook(fd, buf, len, flags, dstaddr, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::write);

    ssize_t ret = sendto_hook(fd, buf, len, flags, dstaddr, addrlen);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::write);
            cgo::scheduler::schedule_wait_signal();
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
    if (!cgo::hook::canhook(fd)) {
        return recvfrom_hook(fd, buf, len, flags, srcaddr, addrlen);
    }

    hook_debug(__FUNCTION__);
    auto state = cgo::hook::check_fd_state(fd, cgo::hook::fd_state::read);

    ssize_t ret = recvfrom_hook(fd, buf, len, flags, srcaddr, addrlen);
    if (ret >= 0) {
        return ret;
    }

    if (EAGAIN == errno
        || EWOULDBLOCK == errno) {
        for (;;) {
            M_SET_STATE(state, fd_state::read);
            cgo::scheduler::schedule_wait_signal();
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
    if (cgo::scheduler::cur_coid() == (uint64_t)-1) {
        return sleep_hook(seconds);
    }

    hook_debug(__FUNCTION__);
    cgo::scheduler::schedule_wait(seconds*1000);
    return 0;
}

typedef int (*usleep_hook_t)(useconds_t usec);
static usleep_hook_t usleep_hook = (usleep_hook_t)dlsym(RTLD_NEXT, "usleep");
int usleep(useconds_t microseconds) {
    if (cgo::scheduler::cur_coid() == (uint64_t)-1) {
        return usleep_hook(microseconds);
    }

    hook_debug(__FUNCTION__);
    cgo::scheduler::schedule_wait(microseconds/1000);
    return 0;
}

typedef int (*poll_hook_t)(struct pollfd *fdarray,unsigned long nfds,int timeout);
static poll_hook_t poll_hook = (poll_hook_t)dlsym(RTLD_NEXT,"poll");
int poll(struct pollfd *fdarray, unsigned long nfds, int timeout) {
    auto can = cgo::hook::canhook_poll_select();

    //if (!can || timeout == 0) {
    if (!can) {
        return poll_hook(fdarray, nfds, timeout);
    }

    hook_debug(__FUNCTION__);

    // 切割轮询次数，精度是10毫秒，目前的实现方案性能不太好，只解决了使用的问题
    int loops = 0;
    if (timeout < 0) {
        loops = -1;
    } else if (timeout == 0) {
        // timeout为0，在外部往往都是一个while循环，所以为了避免线程阻塞，需要强制gosleep
        loops = 1;
    } else if (timeout < 10) {
        loops = 1;
    } else {
        loops = timeout % 10 == 0 ? (timeout / 10) : (timeout / 10 + 1);
    }

    for (int i = 0; i < loops || loops == -1; i++) {
        // 内循环1毫秒
        for (int j = 0; j < 10; j++) {
            int r = poll_hook(fdarray, nfds, 0);
            if (r != 0) {
                return r;
            }
            usleep_hook(100);
        }
        //hook_debug("poll wait");
        cgo::scheduler::schedule_wait(10);
    }

    return poll_hook(fdarray, nfds, 0);
}

typedef int (*select_hook_t)(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
static select_hook_t select_hook = (select_hook_t)dlsym(RTLD_NEXT,"select");
int select(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    auto can = cgo::hook::canhook_poll_select();

    if (!can) {
        return select_hook(maxfd, readfds, writefds, exceptfds, timeout);
    }

    hook_debug(__FUNCTION__);

    // 切割轮询次数，精度是10毫秒，目前的实现方案性能不太好，只解决了使用的问题
    int loops = 0;
    if (!timeout) {
        loops = -1;
    } else {
        time_t mills = timeout->tv_sec*1000 + timeout->tv_usec/1000;
        if (mills < 10) {
            loops = 1;
        } else {
            loops = mills % 10 == 0 ? (mills / 10) : (mills / 10 + 1);
        }
    }

    auto op_select = [maxfd, readfds, writefds, exceptfds]()->int {
        fd_set rfs, wfs, efs;
        FD_ZERO(&rfs);
        FD_ZERO(&wfs);
        FD_ZERO(&efs);

        if (readfds) {
            rfs = *readfds;
        }
        if (writefds) {
            wfs = *writefds;
        }
        if (exceptfds) {
            efs = *exceptfds;
        }

        int r = select_hook(maxfd, readfds ? &rfs : 0, writefds ? &wfs : 0, exceptfds ? &efs : 0, 0);
        if (r != 0) {
            if (readfds) {
                *readfds = rfs;
            }
            if (writefds) {
                *writefds = wfs;
            }
            if (exceptfds) {
                *exceptfds = efs;
            }
        }
        return r;
    };

    for (int i = 0; i < loops || loops == -1; i++) {
        // 内循环1毫秒
        for (int j = 0; j < 10; j++) {
            int r = op_select();
            if (r != 0) {
                return r;
            }
            usleep_hook(100);
        }
        cgo::scheduler::schedule_wait(10);
    }

    return op_select();
}

#elif _MSC_VER

#define WIN32_LEAN_AND_MEAN
#define MIN_HOOK_IMPLEMENTATION
#include <WinSock2.h>
#include <MSWSock.h>
#include "minhook.h"
#include "epoll_iocp.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

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

// for more error info,see WSAGetLastError
typedef SOCKET(PASCAL FAR *accept_hook_t)(
    _In_ SOCKET s,
    _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR* addr,
    _Inout_opt_ int FAR* addrlen);
static accept_hook_t accept_hook = 0;
SOCKET PASCAL FAR hook_accept (
                          _In_ SOCKET s,
                          _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
                          _Inout_opt_ int FAR *addrlen) {
    if (!cgo::hook::canhook((int)s)) {
        return accept_hook(s, addr, addrlen);
    }

    hook_debug(__FUNCTION__);
    
    for (;;) {
        SOCKET c_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (c_fd == INVALID_SOCKET) {
            return INVALID_SOCKET;
        }

        AcceptOverlapped* ov = new AcceptOverlapped;
        memset(ov, 0, sizeof(AcceptOverlapped));
        ov->l_fd = s;
        ov->c_fd = c_fd;

        auto state = cgo::hook::check_fd_state((int)s, cgo::hook::fd_state::accept);
        M_SET_STATE(state, fd_state::accept);

        int addr = sizeof(sockaddr_in) + 16;
        int ret = AcceptEx(ov->l_fd,
            ov->c_fd,
            ov->addr_buf,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            &ov->bytes, (LPOVERLAPPED)ov);

        int err = ERROR_SUCCESS;

        do {
            if (ret == FALSE) {
                err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            void* data;
            cgo::scheduler::schedule_wait_signal(data);
            err = ov->err;
        } while (false);

        state->flag ^= cgo::hook::fd_state::accept;
        delete ov;

        if (err == ERROR_SUCCESS) {
            return c_fd;
        }

        closesocket(c_fd);
        if (err != WSAECONNRESET) {
            WSASetLastError(err);
            break;
        }
    }
    
    return INVALID_SOCKET;
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
    if (!cgo::hook::canhook((int)s)) {
        return connect_hook(s, name, namelen);
    }

    hook_debug(__FUNCTION__);

    DWORD dwBytes;
    LPFN_CONNECTEX lpfnConn = NULL;
    GUID guidConnectEx = WSAID_CONNECTEX;
    if (SOCKET_ERROR == WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidConnectEx, sizeof(guidConnectEx), &lpfnConn, sizeof(lpfnConn), &dwBytes, NULL, NULL))
    {
        return -1;
    }

    for (;;) {
        ConnOverlapped* ov = new ConnOverlapped;
        memset(ov, 0, sizeof(ConnOverlapped));
        ov->name = name;
        ov->namelen = namelen;

        auto state = cgo::hook::check_fd_state((int)s, cgo::hook::fd_state::connect);
        M_SET_STATE(state, fd_state::connect);

        auto ret = lpfnConn(s, ov->name, ov->namelen, NULL, 0, NULL, (LPOVERLAPPED)ov);
        int err = ERROR_SUCCESS;

        do {
            if (ret == FALSE) {
                err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            void* data;
            cgo::scheduler::schedule_wait_signal(data);
        } while (false);

        delete ov;
        state->flag ^= cgo::hook::fd_state::connect;
        if (state->flag & cgo::hook::fd_state::connecok) {
            return 0;
        }

        WSASetLastError(WSAENETUNREACH);
        break;
    }

    return -1;
}

typedef int (PASCAL FAR* closesocket_hook_t)(IN SOCKET s);
static closesocket_hook_t closesocket_hook = 0;
int PASCAL FAR hook_closesocket (IN SOCKET s) {
    auto state = cgo::hook::get_fd_state((int)s);
    if (state->flag & cgo::hook::fd_state::set) {
        hook_debug(__FUNCTION__);
        cgo::hook::clear_fd_state(s);
    }
    return closesocket_hook(s);
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
    if (!cgo::hook::canhook(s)) {
        return sendto_hook(s, buf, len, flags, to, tolen);
    }

    hook_debug(__FUNCTION__);

    for (;;) {
        SendtoOverlapped* ov = new SendtoOverlapped;
        memset(ov, 0, sizeof(SendtoOverlapped));
        ov->buf[0].buf = const_cast<char*>(buf);
        ov->buf[0].len = len;
        ov->name = to;
        ov->namelen = tolen;

        auto state = cgo::hook::check_fd_state((int)s, cgo::hook::fd_state::write);
        M_SET_STATE(state, fd_state::write);

        int ret = WSASendTo(s, ov->buf, 1, &ov->bytes, flags, ov->name, ov->namelen, (LPOVERLAPPED)ov, NULL);
        int err = ERROR_SUCCESS;

        do {
            if (ret != ERROR_SUCCESS) {
                err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            void* data;
            cgo::scheduler::schedule_wait_signal(data);
            err = ov->err;
        } while (false);

        state->flag ^= cgo::hook::fd_state::write;
        DWORD bytes = ov->bytes;
        delete ov;

        if (err != ERROR_SUCCESS) {
            WSASetLastError(err);
        }
        return bytes;
    }
    return -1;
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
    if (!cgo::hook::canhook(s)) {
        return recvfrom_hook(s, buf, len, flags, from, fromlen);
    }

    hook_debug(__FUNCTION__);

    for (;;) {
        RecvfromOverlapped* ov = new RecvfromOverlapped;
        memset(ov, 0, sizeof(RecvfromOverlapped));
        ov->buf[0].buf = buf;
        ov->buf[0].len = len;
        ov->name = from;
        ov->namelen = fromlen;

        auto state = cgo::hook::check_fd_state((int)s, cgo::hook::fd_state::read);
        M_SET_STATE(state, fd_state::read);

        DWORD flag = 0;
        int ret = WSARecvFrom(s, ov->buf, 1, &ov->bytes, &flag, ov->name, ov->namelen, (LPOVERLAPPED)ov, NULL);
        int err = ERROR_SUCCESS;

        do {
            if (ret != ERROR_SUCCESS) {
                err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            void* data;
            cgo::scheduler::schedule_wait_signal(data);
            err = ov->err;
        } while (false);

        state->flag ^= cgo::hook::fd_state::read;
        DWORD bytes = ov->bytes;
        delete ov;

        if (err != ERROR_SUCCESS) {
            WSASetLastError(err);
        }
        return bytes;
    }

    return -1;
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
    if (!cgo::hook::canhook((int)s)) {
        return send_hook(s, buf, len, flags);
    }

    hook_debug(__FUNCTION__);
    
    for (;;) {
        SendOverlapped* ov = new SendOverlapped;
        memset(ov, 0, sizeof(SendOverlapped));
        ov->buf[0].buf = const_cast<char*>(buf);
        ov->buf[0].len = len;

        auto state = cgo::hook::check_fd_state((int)s, cgo::hook::fd_state::write);
        M_SET_STATE(state, fd_state::write);

        int ret = WSASend(s, ov->buf, 1, &ov->bytes, 0, (LPOVERLAPPED)ov, NULL);
        int err = ERROR_SUCCESS;

        do {
            if (ret != ERROR_SUCCESS) {
                err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            void* data;
            cgo::scheduler::schedule_wait_signal(data);
            ov->err = err;
        } while (false);

        state->flag ^= cgo::hook::fd_state::write;
        DWORD bytes = ov->bytes;
        delete ov;

        if (err != ERROR_SUCCESS) {
            WSASetLastError(err);
        }
        return bytes;
    }

    return -1;
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
    if (!cgo::hook::canhook((int)s)) {
        return recv_hook(s, buf, len, flags);
    }

    hook_debug(__FUNCTION__);

    for (;;) {
        RecvOverlapped* ov = new RecvOverlapped;
        memset(ov, 0, sizeof(RecvOverlapped));
        ov->buf[0].buf = buf;
        ov->buf[0].len = len;

        auto state = cgo::hook::check_fd_state((int)s, cgo::hook::fd_state::read);
        M_SET_STATE(state, fd_state::read);

        DWORD flag = 0;
        int ret = WSARecv(s, ov->buf, 1, &ov->bytes, &flag, (LPOVERLAPPED)ov, NULL);
        int err = ERROR_SUCCESS;

        do {
            if (ret != ERROR_SUCCESS) {
                err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            void* data;
            cgo::scheduler::schedule_wait_signal(data);
            ov->err = err;
        } while (false);

        state->flag ^= cgo::hook::fd_state::read;
        DWORD bytes = ov->bytes;
        delete ov;

        if (err != ERROR_SUCCESS) {
            WSASetLastError(err);
        }
        return bytes;
    }

    return -1;
}

typedef int (PASCAL FAR *select_hook_t)(
    _In_ int nfds,
    _Inout_opt_ fd_set FAR* readfds,
    _Inout_opt_ fd_set FAR* writefds,
    _Inout_opt_ fd_set FAR* exceptfds,
    _In_opt_ const struct timeval FAR* timeout
);
static select_hook_t select_hook = 0;
int PASCAL FAR hook_select(_In_ int nfds,
    _Inout_opt_ fd_set FAR* readfds,
    _Inout_opt_ fd_set FAR* writefds,
    _Inout_opt_ fd_set FAR* exceptfds,
    _In_opt_ const struct timeval FAR* timeout) {
    auto can = cgo::hook::canhook_poll_select();
    if (!can) {
        return select_hook(nfds, readfds, writefds, exceptfds, timeout);
    }

    hook_debug(__FUNCTION__);

    // 切割轮询次数，精度是10毫秒，目前的实现方案性能不太好，只解决了使用的问题
    int loops = 0;
    if (!timeout) {
        loops = -1;
    }
    else {
        time_t mills = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
        if (mills < 10) {
            loops = 1;
        }
        else {
            loops = mills % 10 == 0 ? (mills / 10) : (mills / 10 + 1);
        }
    }

    auto op_select = [nfds, readfds, writefds, exceptfds]()->int {
        fd_set rfs, wfs, efs;
        FD_ZERO(&rfs);
        FD_ZERO(&wfs);
        FD_ZERO(&efs);

        if (readfds) {
            rfs = *readfds;
        }
        if (writefds) {
            wfs = *writefds;
        }
        if (exceptfds) {
            efs = *exceptfds;
        }

        int r = select_hook(nfds, readfds ? &rfs : 0, writefds ? &wfs : 0, exceptfds ? &efs : 0, 0);
        if (r != 0) {
            if (readfds) {
                *readfds = rfs;
            }
            if (writefds) {
                *writefds = wfs;
            }
            if (exceptfds) {
                *exceptfds = efs;
            }
        }
        return r;
    };

    for (int i = 0; i < loops || loops == -1; i++) {
        int r = op_select();
        if (r != 0) {
            return r;
        }
        cgo::scheduler::schedule_wait(10);
    }

    return op_select();
}

typedef struct hostent FAR* (PASCAL FAR* gethostbyname_hook_t)(_In_z_ const char FAR* name);
static gethostbyname_hook_t gethostbyname_hook = 0;
struct hostent FAR * PASCAL FAR hook_gethostbyname(_In_z_ const char FAR * name) {
    return gethostbyname_hook(name);
}

typedef void (WINAPI *sleep_hook_t)(_In_ DWORD dwMilliseconds);
static sleep_hook_t sleep_hook = 0;
void WINAPI hook_sleep(_In_ DWORD dwMilliseconds) {
    if (cgo::scheduler::cur_coid() == (uint64_t)-1) {
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
if (MH_EnableHookApi(TEXT(module), #oriname) != MH_OK) {\
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
    HOOK_API("Ws2_32.dll", socket, socket);
    HOOK_API("Ws2_32.dll", accept, accept);
    HOOK_API("Ws2_32.dll", connect, connect);
    HOOK_API("Ws2_32.dll", closesocket, closesocket);
    HOOK_API("Ws2_32.dll", sendto, sendto);
    HOOK_API("Ws2_32.dll", recvfrom, recvfrom);
    HOOK_API("Ws2_32.dll", send, send);
    HOOK_API("Ws2_32.dll", recv, recv);
    HOOK_API("Ws2_32.dll", select, select);

    //HOOK_API2(gethostbyname, gethostbyname);
    return true;
}

#endif

struct hook_init {
    hook_init() {
#ifdef _MSC_VER
        win_hook_init();
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
    }
}
