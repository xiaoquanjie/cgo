//
// 提供对opentelemetry的简单封装,使用http协议(不使用grpc的原因是编译不过,尚不知道原因).
// Created by xiaoqj on 2024/4/17.
//

#pragma once

#include "context.h"
#include <filesystem>

namespace otl {
    typedef opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> TracerPtr;
    typedef std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SpanExporterPtr;
    // exporter列表.
    typedef std::vector<SpanExporterPtr> ExporterContainer;

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

        static void ParseFullMethod(const std::string &fullMethod, std::string &rpcService, std::string &rpcMethod) {
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

        static std::string GetTraceId(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span) {
            const int ksize = opentelemetry::trace::TraceId::kSize*2;
            std::string buf;
            buf.resize(ksize);
            opentelemetry::nostd::span<char, ksize> traceBuf(buf.data(), ksize);
            span->GetContext().trace_id().ToLowerBase16(traceBuf);
            return std::move(buf);
        }

        static std::string GetSpanId(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span) {
            const int ksize = opentelemetry::trace::SpanId::kSize*2;
            std::string buf;
            buf.resize(ksize);
            opentelemetry::nostd::span<char, ksize> traceBuf(buf.data(), ksize);
            span->GetContext().span_id().ToLowerBase16(traceBuf);
            return std::move(buf);
        }

    protected:
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
        std::string tracer_name = "cpp-global-tracer";
        return provider->GetTracer(tracer_name);
    }

    // 从context中创建一个span start options.
    inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
    SpanFromContext(opentelemetry::nostd::string_view name, opentelemetry::trace::SpanKind kind, Context &ctx) {
        opentelemetry::trace::StartSpanOptions options;
        options.parent = opentelemetry::trace::GetSpan(ctx)->GetContext();
        options.kind = kind;
        auto span = GetGlobalTracer()->StartSpan(name, {}, options);
        return span;
    }

    // 默认的grpc服务拦截器.
    class DefaultGrpcServerInterceptor : public cogrpc::ServerInterceptor {
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
    public:
        explicit DefaultGrpcServerInterceptor(grpc::experimental::ServerRpcInfo *info) : cogrpc::ServerInterceptor(info) {}

        void begin() {
            // 分割方法.
            std::string fullMethod = rpcInfo_->method();
            std::string rpcService;
            std::string rpcMethod;
            Otl::ParseFullMethod(fullMethod, rpcService, rpcMethod);
            std::string spanName = rpcService + "/" + rpcMethod;

            auto serverCtx = dynamic_cast<grpc::ServerContext*>(rpcInfo_->server_context());
            // 创建一个新的context.
            Context parentContext;

            // extract data from carrier to context.
            auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
            parentContext = prop->Extract(GrpcServerCarrier(serverCtx), parentContext);

            // 创建一个新的span.
            span_ = SpanFromContext(spanName, opentelemetry::trace::SpanKind::kServer, parentContext);
            // set attribute
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcSystem, "grpc");
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcService, rpcService);
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcMethod, rpcMethod);
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

    public:
        explicit DefaultGrpcClientInterceptor(grpc::experimental::ClientRpcInfo *info) : cogrpc::ClientInterceptor(info) {}

        void begin(std::multimap<std::string, std::string> *metaData) {
            // 分割方法.
            std::string fullMethod = rpcInfo_->method();
            std::string rpcService;
            std::string rpcMethod;
            Otl::ParseFullMethod(fullMethod, rpcService, rpcMethod);
            std::string spanName = rpcService + "/" + rpcMethod;

            Context parentContext;
            auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
            parentContext = prop->Extract(MapGetterCarrier(*metaData), parentContext);

            span_ = SpanFromContext(spanName, opentelemetry::trace::SpanKind::kClient, parentContext);
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcSystem, "grpc");
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcService, rpcService);
            span_->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcMethod, rpcMethod);

            Context currentContext = NewContextFromSpan(span_);

            // inject data to carrier
            auto clientCtx = rpcInfo_->client_context();
            GrpcClientCarrier carrier(clientCtx);
            prop->Inject(carrier, currentContext);
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
            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
            }
            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::PRE_SEND_MESSAGE)) {
                auto mp = methods->GetSendInitialMetadata();
                begin(mp);
            }
            if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_STATUS)) {
                end(*methods->GetRecvStatus());
            }
            methods->Proceed();
        }
    };
};
