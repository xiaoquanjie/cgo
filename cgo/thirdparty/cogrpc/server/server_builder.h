//
// Created by xiaoqj on 2023/6/1.
//

#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/completion_queue.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <cstdio>
#include "serverdata.h"
#include "interceptor.h"

namespace cogrpc {
    class ServerBuilder;
    ServerBuilder* DefSrvBuilder();

    template<class T>
    struct GrpcInitOnce {
       static std::once_flag f_;

       static void Init() {
           std::call_once(f_, []() {
               grpc::EnableDefaultHealthCheckService(true);
               grpc::reflection::InitProtoReflectionServerBuilderPlugin();
           });
       }
    };

    template<class T>
    std::once_flag GrpcInitOnce<T>::f_;

    class IServer {
    public:
        // 子类需要实现.
        virtual void InitMethod() = 0;

        virtual ::grpc::Service* GetService() = 0;
    };

    class ServerBuilder {
    public:
        ServerBuilder() {
            GrpcInitOnce<ServerBuilder>::Init();
            cq_ = builder_.AddCompletionQueue();
        }

        ::grpc::ServerCompletionQueue* GetQueue() {
            return cq_.get();
        }

        // 设置消息的最大大小,单位字节.
        virtual void SetMaxMsgSize(int max_message_size) {
            builder_.SetMaxMessageSize(max_message_size);
        }

        virtual void AddListeningPort(const std::string& addr_uri) {
            if (!addr_uri.empty()) {
                builder_.AddListeningPort(addr_uri,
                                          ::grpc::InsecureServerCredentials(),
                                          nullptr);
            }
        }

        template<class T>
        bool RegisterService() {
            std::string name = typeid(T).name();
            auto iter = service_map_.find(name);
            if (iter != service_map_.end()) {
                return false;
            }

            std::shared_ptr<IServer> p = std::make_shared<T>();
            service_map_[name] = p;

            // 注册服务
            builder_.RegisterService(p->GetService());
            return true;
        }

        // 添加拦截器方法.
        template<class ServerInterceptorType>
        void AddInterceptor() {
            interceptor_factories_.push_back(std::move(CreateServerInterceptorFactory<ServerInterceptorType>()));
        }

        void Run() {
            std::call_once(once_, [this]() {
                this->OnStart();
                CoLooper()([this]() {
                    this->Loop(0);
                });
            });
        }

    protected:
        static void SStop() {
            DefSrvBuilder()->Stop();
        }

        void OnStart() {
            builder_.experimental().SetInterceptorCreators(std::move(interceptor_factories_));
            server_ = builder_.BuildAndStart();
            if (!server_) {
                return;
            }
            for (auto& mp : service_map_) {
                mp.second->InitMethod();
            }

            wg_.Add(1);
        }

        void Stop() {
            // not start
            if (!server_) {
                return;
            }

            printf("[cogrpc] try to stop grpc server\n");
            this->stop_ = true;
            server_->Shutdown();
            StopQueue();
            wg_.Wait();
            printf("[cogrpc] stop grpc server successfully\n");
        }

        void Loop(uint32_t) {
            std::atexit(&ServerBuilder::SStop);
            for (;;) {
                if (this->stop_) {
                    break;
                }
                PickMsg();
            }
            wg_.Done();
        }

        void StopQueue() {
            this->cq_->Shutdown();
        }

        bool PickMsg() {
            void* tag = nullptr;
            bool ok = false;

            bool shutdown = !this->cq_->Next(&tag, &ok);
            if (!tag) {
                return false;
            }

            // ok为true表示事件成功,false表示事件失败.
            static_cast<ICallData*>(tag)->doResponse(shutdown, ok);
            return true;
        }

    protected:
        bool stop_ = false;
        cgo::WaitGroup wg_;
        ::grpc::ServerBuilder builder_;
        std::unique_ptr<::grpc::Server> server_;
        std::unique_ptr<::grpc::ServerCompletionQueue> cq_;
        std::unordered_map<std::string, std::shared_ptr<IServer>> service_map_;
        std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> interceptor_factories_;
        std::once_flag once_;
    };

//////////////////////////////////////////////////////////////////////////////////////////////////////

    inline ServerBuilder* DefSrvBuilder() {
        static auto builder = new ServerBuilder;
        return builder;
    }

}

