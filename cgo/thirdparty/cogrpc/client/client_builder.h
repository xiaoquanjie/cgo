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
#include <cstdlib>
#include "clientdata.h"
#include "channel.h"

namespace cogrpc {
    class ClientBuilder;
    ClientBuilder* DefCliBuilder();

    // 管理着客户端.
    class ClientBuilder {
    public:
        ClientBuilder() {
            wg_.Add(1);
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

        void Stop() {
            this->stop_ = true;
            StopQueue();
            wg_.Wait();
        }

    protected:
        static void SStop() {
            DefCliBuilder()->Stop();
        }

        void Loop(uint32_t) {
            std::atexit(&ClientBuilder::SStop);
            for (;;) {
                if (this->stop_) {
                    break;
                }
                PickMsg();
            }
            wg_.Done();
        }

        void StopQueue() {
            this->cq_.Shutdown();
            // 排干事件.
            while (true) {
                if (!PickMsg()) {
                    break;
                }
            }
        }

        bool PickMsg() {
            void* tag = nullptr;
            bool ok = false;

            bool shutdown = !this->cq_.Next(&tag, &ok);
            if (!tag) {
                return false;
            }

            // ok为true表示事件成功,false表示事件失败.
            static_cast<ICallData*>(tag)->doResponse(shutdown, ok);
            return true;
        }

    private:
        bool stop_ = false;
        cgo::WaitGroup wg_;
        ::grpc::CompletionQueue cq_;
        std::once_flag once_;
    };

    // 默认的客户端builder
    inline ClientBuilder* DefCliBuilder() {
        static ClientBuilder builder;
        return &builder;
    }

}