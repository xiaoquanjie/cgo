//
// Created by xiaoqj on 2024/2/19.
//

#pragma once

#ifdef __GNUC__
#include <sys/socket.h>
#elif _MSC_VER
#include <WinSock2.h>
#endif

#include <stdint.h>
#include <string>
#include "cgo/cgo.h"

namespace co_net {
    class Addr {
    public:
        virtual ~Addr() = default;
        virtual std::string Network() const = 0;
        virtual std::string String() const = 0;
        virtual std::string Ip() const = 0;
        virtual unsigned short Port() const = 0;
    };

    class Conn {
    public:
        virtual ~Conn() = default;
        virtual size_t Read(char* buf, int len) = 0;
        virtual size_t Write(const char* buf, int len) = 0;
        virtual void Close() = 0;
        virtual const Addr& LocalAddr() = 0;
        virtual const Addr& RemoteAddr() = 0;
    };

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual Conn* Accept() = 0;
        virtual void Close() = 0;
        virtual const Addr& LocalAddr() = 0;
    };
}
