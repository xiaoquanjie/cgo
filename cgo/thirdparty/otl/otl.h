//
// 提供对opentelemetry的简单封装,使用http协议(不使用grpc的原因是编译不过,尚不知道原因).
// Created by xiaoqj on 2024/4/17.
//

#pragma once

#include <grpcpp/server_builder.h>
#include <cgo/thirdparty/cogrpc/cogrpc.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_context.h>
#include <opentelemetry/sdk/trace/tracer_context_factory.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <filesystem>

namespace otl {
    typedef opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> TracerPtr;
    typedef std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SpanExporterPtr;
    // exporter列表.
    typedef std::vector<SpanExporterPtr> ExporterContainer;

    class GrpcServerCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        grpc::ServerContext *context_;

    public:
        explicit GrpcServerCarrier(grpc::ServerContext *context) : context_(context) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            auto it = context_->client_metadata().find({key.data(), key.size()});
            if (it != context_->client_metadata().end())
            {
                return it->second.data();
            }
            return "";
        }

        void Set(opentelemetry::nostd::string_view /* key */, opentelemetry::nostd::string_view /* value */) noexcept override {
            // Not required for server
        }
    };

    class GrpcClientCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        grpc::ClientContext *context_;

    public:
        explicit GrpcClientCarrier(grpc::ClientContext *context) : context_(context) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view /* key */) const noexcept override {
            return "";
        }

        void Set(opentelemetry::nostd::string_view key, opentelemetry::nostd::string_view value) noexcept override{
            context_->AddMetadata(std::string(key), std::string(value));
        }
    };

    class Otl {
    public:
        void Init(int argc, char **argv, ExporterContainer& li) {
            if (init_) {
                return;
            }

            parseArguments(argc, argv);

            std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>> processors;
            for (auto&& exp : li) {
                processors.push_back(opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exp)));
            }

            // 设置资源.
            namespace resource = opentelemetry::sdk::resource;
            resource::Resource source = resource::Resource::Create({{"service.name", executable_name_},
                                                                    {resource::SemanticConventions::kProcessExecutablePath, executable_path_},
                                                                    {resource::SemanticConventions::kOsType, ""},
                                                                    {resource::SemanticConventions::kHostName, ""}});
            //source.Merge(opentelemetry::sdk::resource::Resource::GetDefault());

            // 创建provider.
            std::unique_ptr<opentelemetry::sdk::trace::TracerContext> context =
                    opentelemetry::sdk::trace::TracerContextFactory::Create(std::move(processors), source);
            std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
                    opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(context));

            // 设置全局provider.
            opentelemetry::trace::Provider::SetTracerProvider(provider);

            // 设置全局propagator.
            opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
                    opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
                            new opentelemetry::trace::propagation::HttpTraceContext()));
            init_ = true;
        }

        void Clean() {
            std::shared_ptr<opentelemetry::trace::TracerProvider> none;
            opentelemetry::trace::Provider::SetTracerProvider(none);
            init_ = false;
        }

        static void parseFullMethod(const std::string &fullMethod, std::string &rpcService, std::string &rpcMethod) {
            auto pos1 = fullMethod.find_first_of('/', 0);
            if (pos1 == 0) {
                auto pos2 = fullMethod.find_first_of('/', 1);
                rpcService = fullMethod.substr(1, pos2-1);
                rpcMethod = fullMethod.substr(pos2+1, fullMethod.size());
            } else {
                rpcService = fullMethod.substr(0, pos1-1);
                rpcMethod = fullMethod.substr(pos1+1, fullMethod.size());
            }
        }

        void parseArguments(int argc, char **argv) {
            executable_path_ = argv[0];
            std::filesystem::path path(executable_path_);
            executable_name_ = path.filename().string();
        }
    private:
        bool init_ = false;
        std::string executable_name_;
        std::string executable_path_;
    };

    inline Otl*
    getOTLObject() {
        static Otl obj;
        return &obj;
    }

    /*
     * 创建http exporter.
     * @url格式: http://ip:4318/v1/traces
     */
    inline SpanExporterPtr
    CreateHttpExporter(const std::string &url) {
        opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
        opts.url = url;
        return opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(opts);
    }

    // 创建控制台exporter
    inline SpanExporterPtr
    CreateConsoleExporter() {
        return opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    }

    // 创建流exporter.
    inline SpanExporterPtr
    CreateOStreamExporter(std::ostream &out) {
        return opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create(out);
    }

    // 初始化工作.
    inline void Init(int argc, char **argv, ExporterContainer& li) {
        getOTLObject()->Init(argc, argv, li);
    }

    // 清除工作.
    inline void Clean() {
        getOTLObject()->Clean();
    }

    // 获取一个tracer.
    inline TracerPtr
    GetGlobalTracer() {
        auto provider = opentelemetry::trace::Provider::GetTracerProvider();
        // @tracer_name是instr-lib的值
        std::string tracer_name = "global-tracer";
        return provider->GetTracer(tracer_name);
    }

    // 默认的grpc服务拦截器.
    class DefaultGrpcServerInterceptor : public cogrpc::ServerInterceptor {
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
        opentelemetry::trace::Scope *scope_;

    public:
        ~DefaultGrpcServerInterceptor() override {
            delete scope_;
        }

        explicit DefaultGrpcServerInterceptor(grpc::experimental::ServerRpcInfo *info) : cogrpc::ServerInterceptor(info), scope_(nullptr) {}

        void begin() {
            // 创建一个span.
            auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
            auto currentCtx = opentelemetry::context::RuntimeContext::GetCurrent();
            auto serverCtx = dynamic_cast<grpc::ServerContext*>(rpcInfo_->server_context());
            auto newContext = prop->Extract(GrpcServerCarrier(serverCtx), currentCtx);

            opentelemetry::trace::StartSpanOptions options;
            options.kind = opentelemetry::trace::SpanKind::kServer;
            options.parent = opentelemetry::trace::GetSpan(newContext)->GetContext();

            // 分割方法.
            std::string fullMethod = rpcInfo_->method();
            std::string rpcService;
            std::string rpcMethod;
            Otl::parseFullMethod(fullMethod, rpcService, rpcMethod);

            std::string spanName = rpcService + "/" + rpcMethod;
            span_ = GetGlobalTracer()->StartSpan(spanName,
                                                     {{opentelemetry::trace::SemanticConventions::kRpcSystem, "grpc"},
                                                      {opentelemetry::trace::SemanticConventions::kRpcService, rpcService},
                                                      {opentelemetry::trace::SemanticConventions::kRpcMethod, rpcMethod}},
                                                     options);

            scope_ = new opentelemetry::trace::Scope(GetGlobalTracer()->WithActiveSpan(span_));
        }

        void end(const grpc::Status& status) {
            if (status.ok()) {
                span_->SetStatus(opentelemetry::trace::StatusCode::kOk);
            } else {
                span_->SetStatus(opentelemetry::trace::StatusCode::kError);
            }
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcGrpcStatusCode, (int)status.error_code());
            span_->End();
        }

        void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
            if (rpcInfo_->type() != grpc::experimental::ServerRpcInfo::Type::UNARY) {
                methods->Proceed();
                return;
            }

            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_MESSAGE)) {
                // 收到消息.
                begin();
            }
            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_STATUS)) {
                // 发送结束.
                end(methods->GetSendStatus());
            }
            methods->Proceed();
        }
    };

    // 默认的grpc客户端拦截器.
    class DefaultGrpcClientInterceptor : public cogrpc::ClientInterceptor {
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
        opentelemetry::trace::Scope *scope_;

    public:
        ~DefaultGrpcClientInterceptor() override {
            delete scope_;
        }

        explicit DefaultGrpcClientInterceptor(grpc::experimental::ClientRpcInfo *info) : cogrpc::ClientInterceptor(info), scope_(nullptr) {}

        void begin() {
            // 创建一个span.
            auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
            auto currentCtx = opentelemetry::context::RuntimeContext::GetCurrent();
            auto clientCtx = rpcInfo_->client_context();
            GrpcClientCarrier carrier(clientCtx);
            prop->Inject(carrier, currentCtx);

            opentelemetry::trace::StartSpanOptions options;
            options.kind = opentelemetry::trace::SpanKind::kClient;

            std::string fullMethod = rpcInfo_->method();
            std::string rpcService;
            std::string rpcMethod;
            Otl::parseFullMethod(fullMethod, rpcService, rpcMethod);

            std::string spanName = rpcService + "/" + rpcMethod;
            span_ = GetGlobalTracer()->StartSpan(spanName,
                                                 {{opentelemetry::trace::SemanticConventions::kRpcSystem, "grpc"},
                                                  {opentelemetry::trace::SemanticConventions::kRpcService, rpcService},
                                                  {opentelemetry::trace::SemanticConventions::kRpcMethod, rpcMethod}},
                                                 options);

            scope_ = new opentelemetry::trace::Scope(GetGlobalTracer()->WithActiveSpan(span_));
        }

        void end(const grpc::Status& status) {
            if (status.ok()) {
                span_->SetStatus(opentelemetry::trace::StatusCode::kOk);
            } else {
                span_->SetStatus(opentelemetry::trace::StatusCode::kError);
            }
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcGrpcStatusCode, (int)status.error_code());
            span_->End();
        }

        void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
            if (rpcInfo_->type() != grpc::experimental::ClientRpcInfo::Type::UNARY) {
                methods->Proceed();
                return;
            }

            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_MESSAGE)) {
                // 发消息前.
                begin();
            }
            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_STATUS)) {
                end(*methods->GetRecvStatus());
            }
            methods->Proceed();
        }
    };
};
