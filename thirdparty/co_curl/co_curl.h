//
// Created by xiaoqj on 2024/1/10.
//

#pragma once

#include <curl/curl.h>
#include <mutex>
#include <memory>
#include <unordered_map>

class co_curl {
public:
    enum HTTP_METHOD {
        METHOD_GET = 0,
        METHOD_POST,
        METHOD_PUT,
    };

    struct Option {
        unsigned int op_timeout = 15 * 1000;        // 执行最长时间, 毫秒
        unsigned int conn_timeout = 15 * 1000;      // 连接最长时间，毫秒
    };

    struct Response {
        std::string value;
        int curl_code = 0;      // curl操作的错误码, 成功为CURLE_OK
        int status_code = 0;    // http协议的错误码，成功则为200, 为0则表示服务器没有返回

        bool Ok() {
            return curl_code == CURLE_OK;
        }
    };

    // 返回nullptr说明curl库出错
    static std::shared_ptr<Response> Get(const std::string& url,
                                         const std::unordered_map<std::string,
                                         std::string>& headers = {},
                                         const Option* opt = 0) {
        return doQuest(METHOD_GET, url, "", headers, opt);
    }

    // 返回nullptr说明curl库出错
    static std::shared_ptr<Response> Post(const std::string& url,
                                          const std::string& content,
                                          const std::unordered_map<std::string, std::string>& headers = {},
                                          const Option* opt = 0) {
        return doQuest(METHOD_POST, url, content, headers, opt);
    }

protected:
    co_curl() = delete;
    co_curl(const co_curl&) = delete;
    co_curl& operator=(const co_curl&) = delete;
    ~co_curl() = delete;

    static std::shared_ptr<Response> doQuest(HTTP_METHOD method,
                                             const std::string& url,
                                             const std::string& content,
                                             const std::unordered_map<std::string, std::string>& headers = {},
                                             const Option* opt = 0) {
        init();

        CURL *curl = curl_easy_init();
        if (!curl) {
            return nullptr;
        }

        // curl_easy_perform内部使用了poll/select函数，所以这里要调用cgo_hook_poll_select进行hook
        cgo_hook_poll_select(true);

        std::shared_ptr<Response> response = std::make_shared<Response>();

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, opt ? opt->op_timeout : 15 * 1000);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, opt ? opt->conn_timeout : 15 * 1000);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 120L);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response->value);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteCb);

        if (method == METHOD_POST) {
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, content.size());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());
        }
        else if (method == METHOD_PUT) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, content.size());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());
        }

        // set header
        struct curl_slist* curl_headers = 0;
        curl_headers = curl_slist_append(curl_headers, "connection: keep-alive");
        if (curl_headers) {
            for (const auto& header : headers) {
                curl_headers = curl_slist_append(curl_headers, (header.first + ": " + header.second).c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
        }

        response->curl_code = curl_easy_perform(curl);
        if (response->curl_code == CURLE_OK) {
            response->curl_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);
        }

        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);

        // 关闭hook
        cgo_hook_poll_select(false);
        return response;
    }

    static void init() {
        static std::once_flag flag;
        std::call_once(flag, []() {
            auto code = curl_global_init(CURL_GLOBAL_ALL);
            if (code != CURLE_OK) {
                assert(false && "failed to call curl_global_init");
                throw "failed to call curl_global_init";
            }
        });
    }

    static size_t onWriteCb(void *buffer, size_t size, size_t count, void* param) {
        std::string* s = static_cast<std::string*>(param);
        if (nullptr == s) {
            return 0;
        }

        s->append(reinterpret_cast<char*>(buffer), size * count);
        return size * count;
    }
};
