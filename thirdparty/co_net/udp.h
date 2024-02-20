//
// Created by xiaoqj on 2024/2/20.
//

#pragma once

#include "tcp.h"

namespace co_net {
    using UDPAddr = tcpaddr;

    inline UDPAddr
    ResolveUDPAddr(const std::string& network, const std::string& ip, unsigned short port) {
        return UDPAddr(network, ip, port);
    }

    inline UDPAddr
    EmptyUDPAddr() {
        return UDPAddr("udp");
    }

    class UdpConn {
    protected:
        int fd_;
        std::string nw_;
        cgo::mutex mu_;
        tcpaddr* localaddr_ = 0;

        UdpConn(const UdpConn&) = delete;
        UdpConn& operator=(const UdpConn&) = delete;
    public:
        UdpConn(const std::string& nw,  int fd) : nw_(nw), fd_(fd) {
            cgo_hook_fd(fd_);
        }

        ~UdpConn() {
            Close();
            if (localaddr_) {
                delete localaddr_;
            }
        }

        void Close() {
            ::close(fd_);
        }

        const Addr* LocalAddr() {
            if (localaddr_) {
                return (Addr*)localaddr_;
            }

            mu_.lock();

            if (!localaddr_) {
                auto taddr = new tcpaddr(nw_);
                auto addr_len = taddr->SockAddrLen();
                getsockname(fd_, taddr->SockAddr(), &addr_len);
                localaddr_ = taddr;
            }

            mu_.unlock();
            return (Addr*)localaddr_;
        }

        size_t Read(char* buf, int len, UDPAddr* addr) {
            auto addr_len = addr->SockAddrLen();
            auto cnt = recvfrom(fd_, buf, len, 0, addr->SockAddr(), &addr_len);
            return cnt;
        }

        size_t Write(const char* buf, int len, UDPAddr& addr) {
            mu_.lock();
            auto cnt = sendto(fd_, buf, len, 0, addr.SockAddr(), addr.SockAddrLen());
            mu_.unlock();
            return cnt;
        }
    };
}