//
// Created by xiaoqj on 2024/3/1.
//

#pragma once

#include <grpcpp/grpcpp.h>
#include <vector>
#include <functional>

namespace cogrpc {
    struct ICallData {
        virtual ~ICallData() = default;
        virtual void doRequest() = 0;
        virtual void doResponse(bool, bool) = 0;
    };

    // 全局中间件.
    class ClientMiddles {
    protected:
    public:
        void Add(const std::function<void(::grpc::ClientContext*)>& fn) {
        }
    };

    class ServerMiddles {
        typedef std::function<void(::grpc::ServerContext*, const char*, const char*)> middle;
    protected:
        static std::vector<middle>& before_middles_vec() {
            static std::vector<middle> middles;
            return middles;
        }

        static std::vector<middle>& after_middles_vec() {
            static std::vector<middle> middles;
            return middles;
        }
    public:
        static void Before(const middle& fn) {
            before_middles_vec().push_back(fn);
        }
        static void After(const middle& fn) {
            after_middles_vec().push_back(fn);
        }
        static void DoBefore(::grpc::ServerContext* ctx, const char* fullmethod, const char* sname) {
            for (auto& fn : before_middles_vec()) {
                fn(ctx, fullmethod, sname);
            }
        }
        static void DoAfter(::grpc::ServerContext* ctx, const char* fullmethod, const char* sname) {
            for (auto& fn : after_middles_vec()) {
                fn(ctx, fullmethod, sname);
            }
        }
    };
}

// 客户端协程接口返回值：客户端流模式.
#undef GRPC_CLIENT_WRITER
#define GRPC_CLIENT_WRITER(request, response) \
std::shared_ptr<cogrpc::ClientStreamWriter<request, response, std::shared_ptr<::grpc::ClientAsyncWriter<request>>>>

#undef GRPC_SRV_READER
#define GRPC_SRV_READER(request, response) \
cogrpc::ServerStreamReader<request, response, ::grpc::ServerAsyncReader<response, request>>

#undef GRPC_CLIENT_READER
#define GRPC_CLIENT_READER(request, response) \
std::shared_ptr<cogrpc::ClientStreamReader<request, response, std::shared_ptr<::grpc::ClientAsyncReader<response>>>>

#undef GRPC_SRV_WRITER
#define GRPC_SRV_WRITER(request, response) \
cogrpc::ServerStreamWriter<request, response, ::grpc::ServerAsyncWriter<response>>

#undef GRPC_CLIENT_RW
#define GRPC_CLIENT_RW(request, response) \
std::shared_ptr<cogrpc::ClientStreamReaderWriter<request, response, std::shared_ptr<::grpc::ClientAsyncReaderWriter<request, response>>>>

#undef GRPC_SRV_RW
#define GRPC_SRV_RW(request, response) cogrpc::ServerStreamReaderWriter<request, response, ::grpc::ServerAsyncReaderWriter<response, request>>
