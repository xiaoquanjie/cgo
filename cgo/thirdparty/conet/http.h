//
// Created by xiaoqj on 2024/2/20.
//

#pragma once

#include "tcp.h"
#include <functional>
#include <cgo/common/http_parser.h>

namespace conet {
    struct HttpResponse;
    inline void finishRequest(HttpResponse* rsp);

    struct HttpRequest {
        bool complete_ = false;
        std::string_view tmp_field_;
        std::string_view tmp_value_;
        std::string_view url_;
        std::string_view body_;
        std::string_view method_;
        std::unordered_multimap<std::string_view, std::string_view> header_;

        [[nodiscard]]
        const std::string_view& Url() const {
            return url_;
        }

        const std::string_view* Header(const std::string_view& view) {
            auto iter = header_.find(view);
            if (iter == header_.end()) {
                return nullptr;
            }
            return &iter->second;
        }

        [[nodiscard]]
        const std::unordered_multimap<std::string_view, std::string_view>& Header() const {
            return header_;
        }

        [[nodiscard]]
        const std::string_view& Body() const {
            return body_;
        }

        [[nodiscard]]
        const std::string_view& Method() const {
            return method_;
        }

        void clear() {
            complete_ = false;
            method_ = body_ = url_ = tmp_value_ = tmp_field_ = std::string_view();
            header_.clear();
        }
    };

    struct HttpResponse {
        friend void finishRequest(HttpResponse*);
    protected:
        TcpConn* conn_;
        std::unordered_map<std::string_view, std::string_view> header_;
        std::string data_;
        int status_ = http_status::HTTP_STATUS_OK;

    public:
        explicit HttpResponse(TcpConn* c) : conn_(c) {}

        // 默认会调用WriteHeader
        size_t Write(const char* buf, int len) {
            data_.append(buf, len);
            return len;
        }

        void AddHeader(const std::string_view& key, const std::string_view& value) {
            header_.insert(std::make_pair(key, value));
        }

        void SetHeader(const std::string_view& key, const std::string_view& value) {
            auto iter = header_.find(key);
            if (iter == header_.end()) {
                header_.insert(std::make_pair(key, value));
            } else {
                iter->second = value;
            }
        }

        // 回复
        void WriteHeader(int statusCode) {
            status_ = statusCode;
        }

    protected:
        void finishRequest() {
            std::string data;
            data.reserve(data_.size() + 1024);
            const int bufLen = 1024;
            char buf[bufLen];
            int len = sprintf(buf, "HTTP/1.1 %d OK\\r\\n", status_);
            data.append(buf, len);
            for (auto& kv : header_) {
                len = sprintf(buf, "%s: %s\\r\\n", kv.first.data(), kv.second.data());
                data.append(buf, len);
            }
            sprintf(buf, "Content-Length: %d\\r\\n", (int)data_.size());
            data.append(buf, len);
            data.append("\\r\\n");
            data.append(data_.data(), data_.size());
            conn_->Write(data.c_str(), (int)data.size());
        }
    };

    using HttpHandler = std::function<void(HttpResponse*, HttpRequest*)>;

    class HttpListener {
    protected:
        TcpListener listener_;
        HttpHandler handler_;

    public:
        HttpListener(const HttpListener&) = delete;
        HttpListener& operator=(const HttpListener&) = delete;

        HttpListener() = default;

        void Close() {
            listener_.Close();
        }

        const TCPAddr& LocalAddr() {
            return listener_.LocalAddr();
        }

        bool Listen(const std::string& network, const std::string& ip, unsigned short port, const HttpHandler& handler) {
            if (!listener_.Listen(network, ip, port)) {
                return false;
            }

            handler_ = handler;
            cgo::WaitGroup wg;
            wg.Add(1);

            go [this, &wg] {
                for (;;) {
                    auto c = this->listener_.Accept();
                    if (!c) {
                        break;
                    }
                    HttpListener::NewConn(c, this->handler_);
                }
                delete this;
                wg.Done();
            };

            wg.Wait();
            return true;
        }

    protected:
        static void NewConn(TcpConn* c, HttpHandler& handler) {
            go [c, handler] {
                for (;;) {
                    const int buflen = 1024;
                    char buf[buflen];
                    std::string all;
                    all.reserve(buflen);
                    const char* mehtod = nullptr;

                    http_parser parser;
                    http_parser_init(&parser, HTTP_REQUEST);
                    http_parser_settings settings;
                    http_parser_settings_init(&settings);

                    HttpRequest request;
                    request.complete_ = false;
                    HttpResponse response(c);

                    parser.data = &request;
                    settings.on_url = &HttpListener::on_url;
                    settings.on_header_field = &HttpListener::on_header_field;
                    settings.on_header_value = &HttpListener::on_header_value;
                    settings.on_body = &HttpListener::on_body;
                    settings.on_headers_complete = &HttpListener::on_header_complete;
                    settings.on_message_complete = &HttpListener::on_msg_complete;

                    while (true) {
                        auto cnt = c->Read(buf, buflen);
                        if (cnt <= 0) {
                            goto parser_failed;
                        }

                        auto oldsize = all.size();
                        if (oldsize + cnt > all.capacity()) {
                            std::string newall;
                            newall.reserve(all.capacity() + buflen);
                            newall.append(all);
                            newall.append(buf, cnt);
                            all.swap(newall);

                            // re-execute
                            http_parser_init(&parser, HTTP_REQUEST);
                            request.clear();
                            http_parser_execute(&parser, &settings, all.c_str(), all.size());
                            if (!request.complete_) {
                                continue;
                            }
                            if (parser.http_errno != 0) {
                                goto parser_failed;
                            }
                            // finish
                            goto parser_finish;
                        }

                        all.append(buf, cnt);
                        http_parser_execute(&parser, &settings, all.c_str() + oldsize, cnt);

                        if (!request.complete_) {
                            continue;
                        }
                        if (parser.http_errno != 0) {
                            goto parser_failed;
                        }
                        // finish
                        goto parser_finish;
                    }
parser_finish:
                    mehtod = http_method_str((http_method)parser.method);
                    request.method_ = std::string_view(mehtod, strlen(mehtod));
                    handler(&response, &request);
                    finishRequest(&response);
                }
parser_failed:
                //std::cout << "close http connection\n";
                delete c;
            };
        }

        static int on_url(http_parser* parser, const char* at, size_t length) {
            auto request = (HttpRequest*)(parser->data);
            if (request->url_.empty()) {
                request->url_ = std::string_view(at, length);
            } else {
                request->url_ = std::string_view(request->url_.data(), request->url_.size() + length);
            }
            //std::cout << "url:" << length << "\n";
            return 0;
        }

        static int on_header_field(http_parser* parser, const char* at, size_t length) {
            auto request = (HttpRequest*)(parser->data);
            if (request->tmp_value_.data() != nullptr) {
                request->header_.insert(std::make_pair(request->tmp_field_, request->tmp_value_));
                request->tmp_value_ = request->tmp_field_ = std::string_view();
            }

            if (request->tmp_field_.empty()) {
                request->tmp_field_ = std::string_view(at, length);
            } else {
                request->tmp_field_ = std::string_view(request->tmp_field_.data(), request->tmp_field_.size() + length);
            }
            //std::cout << "head field:" << length << "\n";
            return 0;
        }

        static int on_header_value(http_parser* parser, const char* at, size_t length) {
            auto request = (HttpRequest*)(parser->data);
            if (request->tmp_value_.empty()) {
                request->tmp_value_ = std::string_view(at, length);
            } else {
                request->tmp_value_ = std::string_view(request->tmp_value_.data(), request->tmp_value_.size() + length);
            }
            //std::cout << "head value:" << length << "\n";
            return 0;
        }

        static int on_header_complete(http_parser* parser) {
            auto request = (HttpRequest*)(parser->data);
            if (!request->tmp_field_.empty()) {
                request->header_.insert(std::make_pair(request->tmp_field_, request->tmp_value_));
                request->tmp_value_ = request->tmp_field_ = std::string_view();
            }
            return 0;
        }

        static int on_body(http_parser* parser, const char* at, size_t length) {
            auto request = (HttpRequest*)(parser->data);
            if (request->body_.empty()) {
                request->body_ = std::string_view(at, length);
            } else {
                request->body_ = std::string_view(request->body_.data(), request->body_.size() + length);
            }
            return 0;
        }

        static int on_msg_complete(http_parser* parser) {
            auto request = (HttpRequest*)(parser->data);
            request->complete_ = true;
            return 0;
        }
    };

    inline void finishRequest(HttpResponse* rsp) {
        rsp->finishRequest();
    }
}
