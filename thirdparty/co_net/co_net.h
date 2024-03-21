//
// Created by xiaoqj on 2024/2/19.
//

#pragma once

#include "tcp.h"
#include "udp.h"
#include "http.h"

namespace co_net {
    // The network must be "tcp", "tcp4", "tcp6"
    inline TcpListener*
    ListenTcp(const std::string& network, const std::string& ip, unsigned short port) {
        auto listener = new TcpListener;
        if (!listener->Listen(network, ip, port)) {
            delete listener;
            return nullptr;
        }

        return listener;
    }

    // The network must be "udp", "udp4", "udp6"
    inline UdpConn*
    ListenUdp(const std::string& network, const std::string& ip, unsigned short port) {
        UDPAddr addr(network, ip, port);
        int fd = 0;
        if (addr.Ipv4()) {
            fd = socket(AF_INET, SOCK_DGRAM, 0);
        } else {
            fd = socket(AF_INET6, SOCK_DGRAM, 0);
        }

        int optval = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == -1) {
            return 0;
        }

        if (bind(fd, addr.SockAddr(), addr.SockAddrLen()) == -1) {
            return 0;
        }

        return new UdpConn(network, fd);
    }

    inline bool
    ListenHttpAndServe(const std::string& network, const std::string& ip, unsigned short port, HttpHandler handler) {
        auto li = new HttpListener;
        if (!li->Listen(network, ip, port, handler)) {
            delete li;
            return false;
        }
        return true;
    }

    inline TcpConn*
    DialTcp(const std::string& network, const std::string& ip, unsigned short port) {
        TCPAddr addr(network, ip, port);
        int fd = 0;
        if (addr.Ipv4()) {
            fd = socket(AF_INET, SOCK_STREAM, 0);
        } else {
            fd = socket(AF_INET6, SOCK_STREAM, 0);
        }

        cgo_hook_fd(fd);
        int ret = connect(fd, addr.SockAddr(), addr.SockAddrLen());
        if (ret != 0) {
            return 0;
        }

        return new TcpConn(network, fd);
    }

    // The network must be "udp", "udp4", "udp6"
    inline UdpConn*
    DialUdp(const std::string& network) {
        int fd = 0;
        if (network == "udp" || network == "udp4") {
            fd = socket(AF_INET, SOCK_DGRAM, 0);
        } else {
            fd = socket(AF_INET6, SOCK_DGRAM, 0);
        }
        return new UdpConn(network, fd);
    }
}

namespace conet = co_net;