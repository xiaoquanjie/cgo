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
#include "serverdata.h"
#include "interceptor.h"

namespace cogrpc {

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
        ~ServerBuilder() {
            Stop();
        }

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
        void AddInterceptorMethod(InterceptorMethod m) {
            interceptor_methods_vec_.push_back(std::move(m));
        }

        void Run() {
            std::call_once(once_, [this]() {
                this->OnStart();
                CoLooper()([this]() {
                    this->Loop(0);
                });
            });
        }

        void Stop() {
            OnStop();
        }

    protected:
        void OnStart() {
            // 添加拦截器.
            auto creators = InterceptorCreators(interceptor_methods_vec_);
            builder_.experimental().SetInterceptorCreators(std::move(creators));

            server_ = builder_.BuildAndStart();
            if (!server_) {
                return;
            }
            for (auto& mp : service_map_) {
                mp.second->InitMethod();
            }
        }

        virtual void OnStop() {
            if (server_) {
                server_->Shutdown();
            }
            if (cq_) {
                StopQueue(cq_.get());
            }
            this->stop_ = true;
        }

        bool Loop(uint32_t) {
            // uniquely identifies a request.
            void* tag = nullptr;
            bool ok = false;
            bool shutdown = false;

            for (;;) {
                if (this->stop_) {
                    break;
                }
                tag = nullptr;
                ok = false;
                shutdown = false;

                shutdown = !this->cq_->Next(&tag, &ok);
                if (!tag) {
                    assert(false);
                }

                // ok为true表示事件成功,false表示事件失败.
                static_cast<ICallData*>(tag)->doResponse(shutdown, ok);
            }

            return true;
        }

        void StopQueue(::grpc::ServerCompletionQueue* cq) {
            if (!cq) {
                return;
            }

            cq->Shutdown();

            // 排干事件.
            while (true) {
                void* tag = nullptr;  // uniquely identifies a request.
                bool ok = false;
                cq->Next(&tag, &ok);
                if (!tag) {
                    break;
                } else {
                    static_cast<ICallData*>(tag)->doResponse(true, true);
                }
            }
        }

    protected:
        bool stop_ = false;
        ::grpc::ServerBuilder builder_;
        std::unique_ptr<::grpc::Server> server_;
        std::unique_ptr<::grpc::ServerCompletionQueue> cq_;
        std::unordered_map<std::string, std::shared_ptr<IServer>> service_map_;
        // 拦截器方法集合.
        std::vector<InterceptorMethod> interceptor_methods_vec_;
        std::once_flag once_;
    };

//////////////////////////////////////////////////////////////////////////////////////////////////////

inline ServerBuilder* DefSrvBuilder() {
    static auto builder = new ServerBuilder;
    return builder;
}


}

