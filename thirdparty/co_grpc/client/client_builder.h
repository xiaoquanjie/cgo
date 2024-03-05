//
// Created by xiaoqj on 2023/5/17.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <string>
#include <unordered_map>
#include <cassert>
#include <memory>
#include <chrono>
#include "clientdata.h"
#include "channel.h"

namespace co_grpc {

// 管理着客户端
class ClientBuilder {
public:
    ~ClientBuilder() {
        cq_.Shutdown();
    }

    ::grpc::CompletionQueue* GetQueue() {
        return &this->cq_;
    }

    void StartLoop() {
        std::call_once(once_, [this](){
             CoLooper()([this](){
                 this->Loop(0);
             });
        });
    }

protected:
    bool Loop(uint32_t mil) {
        void* tag = 0;
        bool ok = false;
        ::grpc::CompletionQueue::NextStatus status;

        if (mil == 0) {
            gpr_timespec deadline;
            deadline.tv_sec = 0;
            deadline.tv_nsec = 0;
            deadline.clock_type = GPR_CLOCK_MONOTONIC;
            status = cq_.AsyncNext(&tag, &ok, deadline);
        } else {
            auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(mil);
            status = cq_.AsyncNext(&tag, &ok, deadline);
        }

        if (status == grpc::CompletionQueue::TIMEOUT) {
            return false;
        }
        if (!tag) {
            return true;
        }

        // log("status %d", ok);

        bool shutdown = status == ::grpc::CompletionQueue::SHUTDOWN;
        static_cast<ICallData*>(tag)->doResponse(shutdown, ok);
        return true;
    }

private:
    ::grpc::CompletionQueue cq_;
    std::once_flag once_;
};

// 默认的客户端builder
inline ClientBuilder* DefCliBuilder() {
    static ClientBuilder builder;
    return &builder;
}

}