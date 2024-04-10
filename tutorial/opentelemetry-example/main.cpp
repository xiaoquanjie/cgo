// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

// Make sure to include GRPC headers first because otherwise Abseil may create
// ambiguity with `nostd::variant` if compiled with Visual Studio 2015. Other
// modern compilers are unaffected.
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "trace_common.h"
#include "common/helloworld.grpc.pb.h"
#include <cgo/thirdparty/cogrpc/cogrpc.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;


namespace
{
    namespace context = opentelemetry::context;
    using namespace opentelemetry::trace;

    class GreeterClient : public cogrpc::Client<helloworld::Greeter>
    {
    public:
        ::grpc::Status SayHello(std::shared_ptr<::grpc::ClientContext> ctx, helloworld::HelloRequest req, helloworld::HelloReply* res)
        {
            StartSpanOptions options;
            options.kind = SpanKind::kClient;

            std::string span_name = "GreeterClient/Greet";
            auto span             = get_tracer("grpc")->StartSpan(
                    span_name,
                    {{SemanticConventions::kRpcSystem, "grpc"},
                     {SemanticConventions::kRpcService, "grpc-example.GreetService"},
                     {SemanticConventions::kRpcMethod, "Greet"},
                     {SemanticConventions::kNetworkPeerAddress, "ip"},
                     {SemanticConventions::kNetworkPeerPort, ""}},
                    options);

            auto scope = get_tracer("grpc-client")->WithActiveSpan(span);

            // inject current context to grpc metadata
            auto current_ctx = context::RuntimeContext::GetCurrent();
            GrpcClientCarrier carrier(ctx.get());
            auto prop = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
            prop->Inject(carrier, current_ctx);

            // Send request to server
            Status status = this->Send<helloworld::HelloRequest, helloworld::HelloReply>(&Stub::PrepareAsyncSayHello, std::move(ctx), req, res);
            if (status.ok())
            {
                span->SetStatus(StatusCode::kOk);
                span->SetAttribute(SemanticConventions::kRpcGrpcStatusCode, status.error_code());
                // Make sure to end your spans!
                span->End();
            }
            else
            {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                span->SetStatus(StatusCode::kError);
                span->SetAttribute(SemanticConventions::kRpcGrpcStatusCode, status.error_code());
                // Make sure to end your spans!
                span->End();
            }
            return status;
        }

    private:

    };  // GreeterClient class

    void RunClient(uint16_t port)
    {
        GreeterClient greeter;
    }
}  // namespace

int main(int argc, char **argv)
{
    InitTracer();
    // set global propagator
    context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
            opentelemetry::nostd::shared_ptr<context::propagation::TextMapPropagator>(
                    new propagation::HttpTraceContext()));
    constexpr uint16_t default_port = 8800;
    uint16_t port;
    if (argc > 1)
    {
        port = atoi(argv[1]);
    }
    else
    {
        port = default_port;
    }
    RunClient(port);
    CleanupTracer();
    return 0;
}