//
// Created by xiaoqj on 2023/8/8.
// 服务器拦截器
//

#pragma once

#include <grpcpp/server_builder.h>
#include <vector>

namespace cogrpc {
    typedef std::function<void(grpc::experimental::ServerRpcInfo *, grpc::experimental::InterceptorBatchMethods *)> InterceptorMethod;

    class ServerInterceptor : public grpc::experimental::Interceptor {
    public:
        ServerInterceptor(grpc::experimental::ServerRpcInfo *info, const InterceptorMethod& method) {
            this->info_ = info;
            this->m_ = method;
        }

        void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override {
            if (!methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_MESSAGE)) {
                methods->Proceed();
                return;
            }

            m_(info_, methods);
            //methods->Proceed();
            //methods->Hijack();
        }

    protected:
        InterceptorMethod m_;
        grpc::experimental::ServerRpcInfo *info_;
    };

    class ServerInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
    public:
        explicit ServerInterceptorFactory(const InterceptorMethod& method) {
            m_ = method;
        }

        grpc::experimental::Interceptor *CreateServerInterceptor(
                grpc::experimental::ServerRpcInfo *info) override
        {
            return new ServerInterceptor(info, m_);
        }

    private:
        InterceptorMethod m_;
    };

    std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
    inline InterceptorCreators(const std::vector<InterceptorMethod>& methods) {
        std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> creators;
        for (auto& m : methods) {
            creators.push_back(std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>(
                    new ServerInterceptorFactory(m)));
        }
        return creators;
    }
}



