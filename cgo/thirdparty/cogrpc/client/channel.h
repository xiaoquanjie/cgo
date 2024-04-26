//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <unordered_map>
#include <regex>
#include <shared_mutex>
#include "interceptor.h"

namespace cogrpc {

// 协程安全的管理器
template<class T>
class ChannelManger {
public:
    // target的模式:
    // 1) dns:[//authority/]host[:port][,host[:port]] #port默认是443
    // 2) ipv4:ip[:port][,ip[:port]] #port默认是443
    // 3) ipv6:ip[:port][,ip[:port]] #port默认是443
    // 4) unix:/文件路径
    std::shared_ptr<T> Get(const std::string& target) {
        // 默认的策略
        return Get(target, "");
    }

    // @lb_policy负载策略：round_robin, pick_first, queue_once
    std::shared_ptr<T> Get(const std::string& target, const std::string& lb_policy) {
        std::shared_lock<std::shared_mutex> lock(mu_);
        auto iter = channel_map_.find(target);
        if (iter != channel_map_.end()) {
            return iter->second;
        }
        return create(target, lb_policy);
    }

    std::shared_ptr<T> GetWithInterceptor(std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>> factories,
                                          const std::string& target,
                                          const std::string& lb_policy) {
        std::shared_lock<std::shared_mutex> lock(mu_);
        // 这就意味着在拦截器个数相同的情况下无法通过拦截器类型正确区分.
        auto id = target + "-interceptor-" + std::to_string(factories.size());
        auto iter = channel_map_.find(id);
        if (iter != channel_map_.end()) {
            return iter->second;
        }
        return createWithInterceptor(std::move(factories), id, target, lb_policy);
    }

    void Del(const std::string& target) {
        channel_map_.erase(target);
    }

    static ChannelManger<T>* Instance() {
        static ChannelManger<T> mgr;
        return &mgr;
    }

protected:
    ChannelManger() {
    }

    std::string calcTarget(const std::string& target) {
        auto pos = target.find_first_of("dns:");
        if (pos == 0) {
            return target;
        }

        pos = target.find_first_of("ipv4:");
        if (pos == 0) {
            return target;
        }

        pos = target.find_first_of("ipv6:");
        if (pos == 0) {
            return target;
        }

        pos = target.find_first_of("unix:");
        if (pos == 0) {
            return target;
        }

        if (isIpv4(target)) {
            return "ipv4:" + target;
        } else {
            return "dns:" + target;
        }
    }

    void split(const std::string &source,
               const std::string &separator,
               std::vector<std::string> &array) {
        array.clear();
        std::string::size_type start = 0;
        while (true) {
            std::string::size_type pos = source.find(separator, start);
            if (pos == std::string::npos) {
                std::string sub = source.substr(start, source.size());
                array.push_back(sub);
                break;
            }

            std::string sub = source.substr(start, pos - start);
            start = pos + separator.size();
            array.push_back(sub);
        }
    }

    bool isIpv4(const std::string& addr) {
        std::vector<std::string> arr;
        split(addr, ":", arr);
        if (arr.size() != 2) {
            return false;
        }

        std::regex pattern("^((\\d{1,2}|1\\d{2}|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d{2}|2[0-4]\\d|25[0-5])$");
        return std::regex_match(arr[0].c_str(), pattern);
    }

    ::grpc::ChannelArguments makeChannelArguments(const std::string& lb_policy) {
        ::grpc::ChannelArguments args;
        // 最大的重连间隔.
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 100);
        // 最小的重连间隔.
        args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 100);
        // 重连间隔.
        args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
        // 重试.
        args.SetInt(GRPC_ARG_ENABLE_RETRIES, 1);
        // 设置包体大小.
        args.SetMaxReceiveMessageSize(1024*1024*10);
        args.SetMaxSendMessageSize(1024*1024*10);

        if (!lb_policy.empty()) {
            args.SetLoadBalancingPolicyName(lb_policy);
        }
        return args;
    }

    std::shared_ptr<T> create(const std::string& target, const std::string& lb_policy) {
        ::grpc::ChannelArguments args = makeChannelArguments(lb_policy);
        auto newTarget = calcTarget(target);
        auto c = ::grpc::CreateCustomChannel(newTarget, ::grpc::InsecureChannelCredentials(), args);
        channel_map_[target] = c;
        return c;
    }

    std::shared_ptr<T> createWithInterceptor(std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>> factories,
                                             const std::string& id,
                                             const std::string& target,
                                             const std::string& lb_policy) {
        ::grpc::ChannelArguments args = makeChannelArguments(lb_policy);
        auto newTarget = calcTarget(target);
        auto c = grpc::experimental::CreateCustomChannelWithInterceptors(newTarget, ::grpc::InsecureChannelCredentials(), args, std::move(factories));
        channel_map_[id] = c;
        return c;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<T>> channel_map_;
    std::shared_mutex mu_;
};

using Channels = ChannelManger<::grpc::Channel>;

inline auto GetChannel(const std::string& target) {
    return Channels::Instance()->Get(target);
}

inline auto GetChannel(const std::string& target, const std::string& lb_policy) {
    return Channels::Instance()->Get(target, lb_policy);
}

inline auto GetChannelWithInterceptor(std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>> factories,
                                      const std::string& target,
                                      const std::string& lb_policy) {
    return Channels::Instance()->GetWithInterceptor(std::move(factories), target, lb_policy);
}

inline void DelChannel(const std::string& target) {
    Channels::Instance()->Del(target);
}

}
