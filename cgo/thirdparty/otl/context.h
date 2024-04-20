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
    using Span = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>;

    // grpc server carrier
    class grpcServerCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        grpc::ServerContext *context_;

    public:
        explicit grpcServerCarrier(grpc::ServerContext *context) : context_(context) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            auto it = context_->client_metadata().find({key.data(), key.size()});
            if (it != context_->client_metadata().end())
            {
                return {it->second.data(), it->second.size()};
            }
            return "";
        }

        void Set(opentelemetry::nostd::string_view /* key */, opentelemetry::nostd::string_view /* value */) noexcept override {
            // Not required for server
        }
    };

    // grpc client carrier
    class grpcClientCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        grpc::ClientContext *context_;

    public:
        explicit grpcClientCarrier(grpc::ClientContext *context) : context_(context) {}

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
    class httpCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        T &headers_;
    public:
        explicit httpCarrier(T &headers) : headers_(headers) {}

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
                return {iter->second.data(), iter->second.size()};
            }
            return "";
        }

        void Set(opentelemetry::nostd::string_view key, opentelemetry::nostd::string_view value) noexcept override {
            headers_[std::string(key.data(), key.size())] = std::string(value.data(), value.size());
        }
    };

    // map getter carrier
    template<typename T>
    class mapGetterCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        T &map_;
    public:
        explicit mapGetterCarrier(T &m) : map_(m) {}

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
                return {iter->second.data(), iter->second.size()};;
            }
            return "";
        }

        void Set(opentelemetry::nostd::string_view key,
                 opentelemetry::nostd::string_view value) noexcept override {
        }
    };

    template<typename T>
    class mapSetterCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        T &map_;
    public:
        explicit mapSetterCarrier(T &m) : map_(m) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            return "";
        }

        void Set(opentelemetry::nostd::string_view key, opentelemetry::nostd::string_view value) noexcept override {

            map_.insert(std::make_pair(std::string(key.data(), key.size()), std::string(value.data(), value.size())));
        }
    };

    template<typename T>
    class mapModifyCarrier : public opentelemetry::context::propagation::TextMapCarrier {
        T &map_;
    public:
        explicit mapModifyCarrier(T &m) : map_(m) {}

        [[nodiscard]]
        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            return "";
        }

        void Set(opentelemetry::nostd::string_view key, opentelemetry::nostd::string_view value) noexcept override {
            typename T::key_type k(key.data(), key.size());
            typename T::key_type v(value.data(), value.size());
            auto iter = map_.find(k);
            if (iter == map_.end()) {
                map_.insert(std::make_pair(k, v));
            } else {
                iter->second = v;
            }
        }
    };

    // 将span的数据写入context.
    inline Context
    NewContextFromSpan(Span &span) {
        return Context {opentelemetry::trace::kSpanKey, span};
    }
}