//
// Created by xiaoqj on 2023/6/1.
// 提供xds_builder与普通builder
//

#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/xds_server_builder.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <chrono>
#include "./calldata.h"
#include "./interceptor.h"

namespace co_grpc {

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
    virtual ~IServer() {}

    virtual void InitMethod() = 0;

    virtual ::grpc::Service* GetService() = 0;

    virtual void SetQueue(::grpc::ServerCompletionQueue* cq) = 0;
};

template<typename T>
class ServerImpl : public IServer {
public:
    ServerImpl() {
        t_ = std::make_shared<T>();
    }

    void InitMethod() override {
        t_->InitMethod();
    }

    ::grpc::Service* GetService() override {
        return (::grpc::Service*)t_->GetService();
    }

    void SetQueue(::grpc::ServerCompletionQueue* cq) override {
        t_->SetQueue(cq);
    }

private:
    std::shared_ptr<T> t_;
};

class ServerBuilder {
public:
    ~ServerBuilder() {
        Stop();
    }

    ServerBuilder() {
        GrpcInitOnce<ServerBuilder>::Init();
    }

    // 设置消息的最大大小, 单位字节
    virtual void SetMaxMsgSize(int max_message_size) {
        builder_.SetMaxMessageSize(max_message_size);
    }

    virtual void AddListeningPort(const std::string& addr_uri) {
        if (addr_uri.size()) {
            builder_.AddListeningPort(addr_uri,
                                      ::grpc::InsecureServerCredentials(),
                                      nullptr);
        }
    }

    template<class T>
    bool RegisterService(bool use_xds = false) {
        if (start_) {
            return false;
        }

        std::string name = typeid(T).name();
        auto iter = service_map_.find(name);
        if (iter != service_map_.end()) {
            return false;
        }

        std::shared_ptr<IServer> p = std::make_shared<ServerImpl<T>>();
        service_map_[name] = p;

        OnReg(p);
        return true;
    }

    // 添加拦截器方法
    void AddInterceptorMethod(InterceptorMethod m) {
        interceptor_methods_vec_.push_back(m);
    }

    bool Loop(uint32_t mil) {
        ServerStatistics();
        if (!start_) {
            start_ = true;
            OnStart();
        }
        if (cq_) {
            return _Loop(cq_.get(), mil);
        }
        return false;
    }

    void Stop() {
        OnStop();
    }

protected:
    virtual void OnReg(std::shared_ptr<IServer> p) {
        // 初始化队列
        OnInitQueue();
        // 设置队列
        p->SetQueue(cq_.get());
        // 注册服务
        builder_.RegisterService(p->GetService());
    }

    virtual void OnStart() {
        // 添加拦截器
        auto creators = InterceptorCreators(interceptor_methods_vec_);
        builder_.experimental().SetInterceptorCreators(std::move(creators));

        server_ = builder_.BuildAndStart();
        if (!server_) {
            log("failed to start grpc server");
            return;
        }
        for (auto& mp : service_map_) {
            mp.second->InitMethod();
        }
    }

    virtual void OnInitQueue() {
        if (!cq_) {
            cq_ = builder_.AddCompletionQueue();
        }
    }

    virtual void OnStop() {
        if (server_) {
            server_->Shutdown();
        }
        if (cq_) {
            StopQueue(cq_.get());
        }
    }

    bool _Loop(::grpc::ServerCompletionQueue* cq, uint32_t mil) {
        if (!cq) {
            return false;
        }

        // uniquely identifies a request.
        void* tag = 0;
        bool ok = false;
        ::grpc::CompletionQueue::NextStatus status;

        if (mil == 0) {
            gpr_timespec deadline;
            deadline.tv_sec = 0;
            deadline.tv_nsec = 0;
            deadline.clock_type = GPR_CLOCK_MONOTONIC;
            status = cq->AsyncNext(&tag, &ok, deadline);
        } else {
            auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(mil);
            status = cq->AsyncNext(&tag, &ok, deadline);
        }

        if (status == grpc::CompletionQueue::TIMEOUT) {
            return false;
        }
        if (!tag) {
            return true;
        }

        // ok为true表示事件成功，false表示事件失败
        bool shutdown = status == ::grpc::CompletionQueue::SHUTDOWN;
        static_cast<IServerData*>(tag)->Proceed(shutdown, ok);
        return true;
    }

    void StopQueue(::grpc::ServerCompletionQueue* cq) {
        if (!cq) {
            return;
        }

        cq->Shutdown();

        // 排干事件
        while (true) {
            void* tag = 0;  // uniquely identifies a request.
            bool ok = false;
            cq->Next(&tag, &ok);
            if (!tag) {
                break;
            } else {
                static_cast<IServerData*>(tag)->Proceed(true, true);
            }
        }
    }

protected:
    bool start_ = false;
    ::grpc::ServerBuilder builder_;
    std::unique_ptr<::grpc::Server> server_;
    std::unique_ptr<::grpc::ServerCompletionQueue> cq_;
    std::unordered_map<std::string, std::shared_ptr<IServer>> service_map_;
    // 拦截器方法集合
    std::vector<InterceptorMethod> interceptor_methods_vec_;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

class XdsServerBuilder : public ServerBuilder {
public:
    void AddListeningPort(const std::string& addr_uri) override {
        if (addr_uri.size()) {
            xds_builder_.AddListeningPort(addr_uri,
                                          ::grpc::InsecureServerCredentials(),
                                          nullptr);
        }
    }

protected:
    void OnReg(std::shared_ptr<IServer> p) override {
        // 初始化队列
        OnInitQueue();
        // 设置队列
        p->SetQueue(cq_.get());
        // 注册服务
        xds_builder_.RegisterService(p->GetService());
    }

    void OnInitQueue() override {
        if (!cq_) {
            cq_ = xds_builder_.AddCompletionQueue();
        }
    }

    void OnStart() override {
        server_ = xds_builder_.BuildAndStart();
        for (auto& mp : service_map_) {
            mp.second->InitMethod();
        }
    }

protected:
    ::grpc::XdsServerBuilder xds_builder_;
    std::unique_ptr<::grpc::Server> xds_server_;
};

inline ServerBuilder* DefSrvBuilder() {
    static ServerBuilder *builder = new ServerBuilder;
    return builder;
}

inline XdsServerBuilder* DefXdsSrvBuilder() {
    static XdsServerBuilder builder;
    return &builder;
}

}

