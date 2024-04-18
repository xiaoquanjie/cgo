//
// Created by xiaoqj on 2023/8/8.
// 服务器拦截器
//

#pragma once

#include <grpcpp/server_builder.h>

namespace cogrpc {
    class ServerInterceptor : public grpc::experimental::Interceptor {
    public:
        explicit ServerInterceptor(grpc::experimental::ServerRpcInfo *info) : rpcInfo_(info) {}

        // void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {}
    protected:
        grpc::experimental::ServerRpcInfo *rpcInfo_;
    };

    template<typename ServerInterceptorType>
    class ServerInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
    public:
        grpc::experimental::Interceptor *CreateServerInterceptor(
                grpc::experimental::ServerRpcInfo *info) override
        {
            return new ServerInterceptorType(info);
        }
    };

    template<typename ServerInterceptorType>
    std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>
    CreateServerInterceptorFactory() {
        return std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>
                (new ServerInterceptorFactory<ServerInterceptorType>);
    }
}



