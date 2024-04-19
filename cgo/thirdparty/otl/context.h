//
// Created by xiaoqj on 2024/4/19.
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

namespace otl {
    // context in opentelemetry
    using Context = opentelemetry::context::Context;

    // grpc server carrier
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

    // grpc client carrier
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

    // http carrier
    template<typename T>
    class HttpCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        T &headers_;
    public:
        explicit HttpCarrier(T &headers) : headers_(headers) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            // Header's first letter seems to be  automatically capitaliazed by our test http-server, so
            // compare accordingly.
            // 兼容大写开头的字符情况.
            auto iter = headers_.find({key.data(), key.size()});
            if (iter == headers_.end()) {
                if (key == opentelemetry::trace::propagation::kTraceParent) {
                    iter = headers_.find("Traceparent");
                } else if (key == opentelemetry::trace::propagation::kTraceState) {
                    iter = headers_.find("Tracestate");
                }
            }

            if (iter != headers_.end()) {
                return iter->second;
            }
            return "";
        }

        void Set(opentelemetry::nostd::string_view key,
                 opentelemetry::nostd::string_view value) noexcept override {
            headers_[std::string(key)] = std::string(value);
        }
    };

    // map getter carrier
    template<typename T>
    class MapGetterCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        T &map_;
    public:
        explicit MapGetterCarrier(T &m) : map_(m) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            // 兼容大写开头的字符情况.
            auto iter = map_.find({key.data(), key.size()});
            if (iter == map_.end()) {
                if (key == opentelemetry::trace::propagation::kTraceParent) {
                    iter = map_.find("Traceparent");
                } else if (key == opentelemetry::trace::propagation::kTraceState) {
                    iter = map_.find("Tracestate");
                }
            }

            if (iter != map_.end()) {
                return iter->second;
            }
            return "";
        }

        void Set(opentelemetry::nostd::string_view key,
                 opentelemetry::nostd::string_view value) noexcept override {
        }
    };

    // 将span的数据写入context.
    inline Context
    NewContextFromSpan(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span) {
        return Context {opentelemetry::trace::kSpanKey, span};
    }
}