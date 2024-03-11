//
// Created by xiaoqj on 2024/2/20.
//

#pragma once

#include "tcp.h"
#include <functional>
#include "cgo/common/http_parser.h"

namespace co_net {
    struct HttpRequest {
        bool complete_;
        std::string_view tmp_field_;
        std::string_view tmp_value_;
        std::string_view url_;
        std::string_view body_;
        std::string_view method_;
        std::unordered_map<std::string_view, std::string_view> header_;

        const std::string_view& Url() {
            return url_;
        }

        const std::string_view* Header(const std::string_view& view) {
            auto iter = header_.find(view);
            if (iter == header_.end()) {
                return 0;
            }
            return &iter->second;
        }

        const std::unordered_map<std::string_view, std::string_view>& Header() const {
            return header_;
        }

        const std::string_view& Body() {
            return body_;
        }

        const std::string_view& Method() {
            return method_;
        }

        void clear() {
            complete_ = false;
            method_ = body_ = url_ = tmp_value_ = tmp_field_ = std::string_view();
            header_.clear();
        }
    };

    struct HttpResponse {
    protected:
        TcpConn* conn_;
    public:
        HttpResponse(TcpConn* c) : conn_(c) {}

        size_t Write(const char* buf, int len) {
            return conn_->Write(buf, len);
        }
    };

    using HttpHandler = std::function<void(HttpResponse*, HttpRequest*)>;

    class HttpListener {
    protected:
        TcpListener listener_;
        HttpHandler handler_;

        HttpListener(const HttpListener&) = delete;
        HttpListener& operator=(const HttpListener&) = delete;
    public:
        HttpListener() {}

        void Close() {
            listener_.Close();
        }

        const TCPAddr& LocalAddr() {
            return listener_.LocalAddr();
        }

        bool Listen(const std::string& network, const std::string& ip, unsigned short port, HttpHandler handler) {
            if (!listener_.Listen(network, ip, port)) {
                return false;
            }

            handler_ = handler;

            go [this] {
                for (;;) {
                    auto c = this->listener_.Accept();
                    if (!c) {
                        continue;
                    }
                    HttpListener::NewConn(c, this->handler_);
                }
                delete this;
            };
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
                    const char* mehtod = 0;

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
                }
parser_failed:
                //std::cout << "close http connection\n";
                delete c;
            };
        }

        static int on_url(http_parser* parser, const char* at, size_t length) {
            HttpRequest* request = (HttpRequest*)parser->data;
            if (request->url_.empty()) {
                request->url_ = std::string_view(at, length);
            } else {
                request->url_ = std::string_view(request->url_.data(), request->url_.size() + length);
            }
            //std::cout << "url:" << length << "\n";
            return 0;
        }

        static int on_header_field(http_parser* parser, const char* at, size_t length) {
            HttpRequest* request = (HttpRequest*)parser->data;
            if (request->tmp_value_.data() != 0) {
                request->header_[request->tmp_field_] = request->tmp_value_;
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
            HttpRequest* request = (HttpRequest*)parser->data;
            if (request->tmp_value_.empty()) {
                request->tmp_value_ = std::string_view(at, length);
            } else {
                request->tmp_value_ = std::string_view(request->tmp_value_.data(), request->tmp_value_.size() + length);
            }
            //std::cout << "head value:" << length << "\n";
            return 0;
        }

        static int on_header_complete(http_parser* parser) {
            HttpRequest* request = (HttpRequest*)parser->data;
            if (!request->tmp_field_.empty()) {
                request->header_[request->tmp_field_] = request->tmp_value_;
                request->tmp_value_ = request->tmp_field_ = std::string_view();
            }
            return 0;
        }

        static int on_body(http_parser* parser, const char* at, size_t length) {
            HttpRequest* request = (HttpRequest*)parser->data;
            if (request->body_.empty()) {
                request->body_ = std::string_view(at, length);
            } else {
                request->body_ = std::string_view(request->body_.data(), request->body_.size() + length);
            }
            return 0;
        }

        static int on_msg_complete(http_parser* parser) {
            HttpRequest* request = (HttpRequest*)parser->data;
            request->complete_ = true;
            return 0;
        }
    };
}
