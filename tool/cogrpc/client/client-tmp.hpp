// 
// 此文件由工具自动生成的，部分需要手动修改 
// Tools built from xiaoqj 
// 

#pragma once 

#include "common/ioid/services.h" 
#include "grpcm/k8s.h" 
#include "grpcm/client/service_client.hpp" 

namespace grpcm  {

    template<class T>
    std::shared_ptr<T> getClient(const ioid::ServiceName& name) {
        auto cli = co_grpc::DefCliBuilder()->Get<T>();
        if (!cli) {
            grpclink::K8sServiceGroup group;
            if (K8s::GetServiceGroup(name, group)) {
                std::string target = "ipv4:" + group.cluster_ip() + ":" + std::to_string(group.port());
                co_grpc::DefCliBuilder()->RegClient<T>(target, "");
                cli = co_grpc::DefCliBuilder()->Get<T>();
            }
        }
        return cli;
    }

    inline std::shared_ptr<TestServiceClient>
    GetTestServiceClient() {
        return getClient<TestServiceClient>(ioid::待填);
    }

    inline std::shared_ptr<HallServiceClient>
    GetHallServiceClient() {
        return getClient<HallServiceClient>(ioid::待填);
    }

}
