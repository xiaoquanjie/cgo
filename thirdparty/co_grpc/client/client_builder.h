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
    bool Loop(uint32_t) {
        // uniquely identifies a request.
        void* tag = 0;
        bool ok = false;
        bool shutdown = false;

        for (;;) {
            tag = 0;
            ok = false;
            shutdown = false;

            shutdown = this->cq_.Next(&tag, &ok) == false;
            if (!tag) {
                assert(false);
            }

            // ok为true表示事件成功，false表示事件失败
            static_cast<ICallData*>(tag)->doResponse(shutdown, ok);
        }

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