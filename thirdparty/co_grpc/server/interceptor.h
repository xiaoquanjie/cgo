//
// Created by xiaoqj on 2023/8/8.
// 服务器拦截器
//

#pragma once

#include <grpcpp/server_builder.h>
#include <vector>

namespace co_grpc {
    typedef std::function<void(grpc::experimental::ServerRpcInfo *, grpc::experimental::InterceptorBatchMethods *)> InterceptorMethod;

    class ServerInterceptor : public grpc::experimental::Interceptor {
    public:
        ServerInterceptor(grpc::experimental::ServerRpcInfo *info, const std::vector<InterceptorMethod>& methods) {
            this->info_ = info;
            method_arr_ = methods;
        }

        void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override {
            if (!methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_MESSAGE)) {
                methods->Proceed();
                return;
            }

            for (auto f : method_arr_) {
                f(info_, methods);
            }
            methods->Proceed();
        }

    protected:
        std::vector<InterceptorMethod> method_arr_;
        grpc::experimental::ServerRpcInfo *info_;
    };

    class ServerInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
    public:
        ServerInterceptorFactory(const std::initializer_list<InterceptorMethod>& methods) {
            for (auto& m : methods) {
                method_arr_.push_back(m);
            }
        }

        ServerInterceptorFactory(const std::vector<InterceptorMethod>& methods) {
            for (auto m : methods) {
                method_arr_.push_back(m);
            }
        }

        grpc::experimental::Interceptor *CreateServerInterceptor(
                grpc::experimental::ServerRpcInfo *info) override
        {
            return new ServerInterceptor(info, method_arr_);
        }

    private:
        std::vector<InterceptorMethod> method_arr_;
    };

    std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
    inline InterceptorCreators(const std::vector<InterceptorMethod>& methods) {
        std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> creators;
        creators.push_back(std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>(
                new ServerInterceptorFactory(methods)));
        return creators;
    }
}



