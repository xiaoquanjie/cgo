//
// Created by xiaoqj on 2024/2/19.
//

#pragma once

#include "net.h"

namespace co_net {
    class tcpaddr : public Addr {
    protected:
        struct sockaddr_storage addr_ = {};
        std::string nw_;

    public:
        tcpaddr(const std::string& network, const std::string& ip, unsigned short port) :nw_(network) {
            if (network == "tcp" || network == "tcp4" || network == "udp" || network == "udp4") {
                struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(&addr_);
                addr->sin_family = AF_INET;
                addr->sin_port = htons(port);
                inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);
            } else if (network == "tcp6" || network == "udp6") {
                struct sockaddr_in6* addr = reinterpret_cast<struct sockaddr_in6*>(&addr_);
                addr->sin6_family = AF_INET6;
                addr->sin6_port = htons(port);
                inet_pton(AF_INET6, ip.c_str(), &addr->sin6_addr);
            } else {
                assert(false);
            }
        }

        tcpaddr(const std::string& network) :nw_(network) {
        }

        ~tcpaddr() override = default;

        sockaddr* SockAddr() {
            return (sockaddr*)&addr_;
        }

        socklen_t SockAddrLen() {
            return sizeof(addr_);
        }

        bool Ipv4() const {
            return addr_.ss_family == AF_INET;
        }

        std::string Network() const override {
            return nw_;
        }

        std::string String() const override {
            auto ip = Ip();
            auto port = Port();
            if (addr_.ss_family == AF_INET) {
                return ip + ":" + std::to_string(port);
            } else {
                return "[" + ip + "]:" + std::to_string(port);
            }
        }

        std::string Ip() const override {
            std::string ip;
            ip.resize(40);
            if (addr_.ss_family == AF_INET) {
                const struct sockaddr_in* addr = reinterpret_cast<const struct sockaddr_in*>(&addr_);
                inet_ntop(AF_INET, &addr->sin_addr, ip.data(), 40);
            } else {
                const struct sockaddr_in6* addr = reinterpret_cast<const struct sockaddr_in6*>(&addr_);
                inet_ntop(AF_INET6, &addr->sin6_addr, ip.data(), 40);
            }
            auto len = strlen(ip.c_str());
            ip.resize(len);
            return ip;
        }

        unsigned short Port() const override {
            if (addr_.ss_family == AF_INET) {
                const struct sockaddr_in* addr = reinterpret_cast<const struct sockaddr_in*>(&addr_);
                return ntohs(addr->sin_port);
            } else {
                const struct sockaddr_in6* addr = reinterpret_cast<const struct sockaddr_in6*>(&addr_);
                return ntohs(addr->sin6_port);
            }
        }
    };

    class tcpconn : public Conn {
    protected:
        int fd_;
        std::string nw_;
        cgo::mutex mu_;
        tcpaddr* localaddr_ = 0;
        tcpaddr* remoteaddr_ = 0;

        tcpconn(const tcpconn&) = delete;
        tcpconn& operator=(const tcpconn&) = delete;
    public:
        tcpconn(const std::string& nw,  int fd) : nw_(nw), fd_(fd) {
            cgo_hook_fd(fd_);
        }

        ~tcpconn() {
            Close();
            if (localaddr_) {
                delete localaddr_;
            }
            if (remoteaddr_) {
                delete remoteaddr_;
            }
        }

        size_t Read(char* buf, int len) override {
            return recv(fd_, buf, len, 0);
        }

        size_t Write(const char* buf, int len) override {
            mu_.lock();
            auto cnt = send(fd_, buf, len, 0);
            mu_.unlock();
            return cnt;
        }

        void Close() override {
            ::close(fd_);
        }

        const Addr& LocalAddr() override {
            if (!localaddr_) {
                mu_.lock();
                if (!localaddr_) {
                    localaddr_ = new tcpaddr(nw_);
                    auto addr_len = localaddr_->SockAddrLen();
                    getsockname(fd_, localaddr_->SockAddr(), &addr_len);
                }
                mu_.unlock();
            }

            return (Addr&)(*localaddr_);
        }

        const Addr& RemoteAddr() override {
            if (!remoteaddr_) {
                mu_.lock();
                if (!remoteaddr_) {
                    remoteaddr_ = new tcpaddr(nw_);
                    auto addr_len = remoteaddr_->SockAddrLen();
                    getpeername(fd_, remoteaddr_->SockAddr(), &addr_len);
                }
                mu_.unlock();
            }

            return (Addr&)(*remoteaddr_);
        }
    };

    class tcplistener : public Listener {
    protected:
        int fd_ = 0;
        tcpaddr* addr_ = 0;

        tcplistener(const tcplistener&) = delete;
        tcplistener& operator=(const tcplistener&) = delete;
    public:
        tcplistener() {

        }

        ~tcplistener() override {
            Close();
            if (addr_) {
                delete addr_;
            }
        }

        // The network must be "tcp", "tcp4", "tcp6"
        bool Bind(const std::string& network, const std::string& ip, unsigned short port) {
            addr_ = new tcpaddr(network, ip, port);
            if (addr_->SockAddr() == 0) {
                return false;
            }

            if (addr_->Ipv4()) {
                fd_ = socket(AF_INET, SOCK_STREAM, 0);
            } else {
                fd_ = socket(AF_INET6, SOCK_STREAM, 0);
            }

            int optval = 1;
            if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == -1) {
                return false;
            }

            if (bind(fd_, addr_->SockAddr(), addr_->SockAddrLen()) == -1) {
                return false;
            }

            if (listen(fd_, 10) == -1) {
                return false;
            }

            cgo_hook_fd(fd_);
            return true;
        }

        Conn* Accept() override {
            struct sockaddr_storage caddr = {};
            socklen_t caddr_len = sizeof(caddr);
            int cfd = accept(fd_, (struct sockaddr*)&caddr, &caddr_len);
            if (cfd == -1) {
                return 0;
            }

            auto conn = new tcpconn(addr_->Network(), cfd);
            return conn;
        }

        void Close() override {
            if (fd_ != 0) {
                ::close(fd_);
                fd_ = 0;
            }
        }

        const Addr& LocalAddr() override {
            if (addr_) {
                return (Addr&)(*addr_);
            }
            static tcpaddr emptyaddr("tcp");
            return (Addr&)emptyaddr;
        }
    };
}
