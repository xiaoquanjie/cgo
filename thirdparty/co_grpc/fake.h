//
// Created by xiaoqj on 2023/5/19.
// 伪头文件
// 为的是解决windows下的ide因不能找到某些头文件而产生的编辑器报红，引发智能提示失败
//

#pragma once

// 是否打开此宏
#define OPEN_FAKE (1)

#ifdef OPEN_FAKE

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

#include <string>

namespace google {
namespace protobuf {
    class Message {
    public:
        std::string ShortDebugString() { return ""; }
    };
}
}

namespace grpc {
enum StatusCode {};
class Status{
public:
    enum {OK, CANCELLED};
    Status() {}
    Status(int) {}
    Status(int, std::string) {}
    bool ok() const {return false;}
    int error_code() const { return 0; }
    const char* error_message() const {return 0;}
};
class Server{
public:
    void Shutdown(){}
};
class CompletionQueue{
public:
    enum NextStatus{SHUTDOWN, TIMEOUT};
    void Shutdown(){}

    template<class P1, class P2, class P3>
    CompletionQueue::NextStatus AsyncNext(P1, P2, P3){}

    template<class p1, class p2>
    void Next(p1, p2) {}
};
class ServerCompletionQueue{
public:
    void Shutdown(){}
    template<class P1, class P2, class P3>
    CompletionQueue::NextStatus AsyncNext(P1, P2, P3){}

    template<class p1, class p2>
    void Next(p1, p2) {}
};
class ServerBuilder{
public:
    template<class p1, class p2, class p3>
    void AddListeningPort(p1, p2, p3) {}

    template<class T>
    void RegisterService(T) {}

    std::unique_ptr<Server> BuildAndStart() {return nullptr;}
    std::unique_ptr<ServerCompletionQueue> AddCompletionQueue() {return nullptr;}
    void SetMaxMessageSize(uint32_t) {}
};
class XdsServerBuilder {
public:
    template<class T>
    void RegisterService(T) {}
    std::unique_ptr<Server> BuildAndStart() {}
    std::unique_ptr<ServerCompletionQueue> AddCompletionQueue() {return nullptr;}
    void SetMaxMessageSize(uint32_t) {}

    template<class p1, class p2, class p3>
    void AddListeningPort(p1, p2, p3) {}
};
class ClientContext{
public:
    template<class T>
    void set_deadline(T) {}
    int deadline() { return 0;}
};
class ServerContext{
public:
    template<class T>
    void AsyncNotifyWhenDone(T) {}
};
class Channel{};
template<class T> class ClientWriter{
public:
    template<class P1>
    bool Write(P1) {}
    bool WritesDone() {}
    Status Finish(){}
};
template<class T> class ClientReader{
public:
    template<class P>
    bool Read(P) {}
    Status Finish(){}
};
template<class T> class ClientAsyncResponseReader{};
template<class T> class ClientAsyncWriter{
public:
    template<class P1, class P2>
    void Write(P1,P2) {}

    template<class P>
    void StartCall(P) {}
};
template<class T1, class T2> class ClientReaderWriter{
public:
    template<class T>
    void Write(T) {}

    void WritesDone(){}

    template<class T>
    bool Read(T) {return false;}

    Status Finish() {}
};
template<class T> class ServerAsyncResponseWriter{};
template<class T1, class T2> class ServerAsyncReader{
public:
    template<class P1, class P2>
    void Read(P1, P2){}
};
class ChannelArguments{};
template<class T> class ServerAsyncWriter{
public:
    template<class P1, class P2>
    void Write(P1,P2) {}
};
template<class T> class ClientAsyncReader{
public:
    template<class P1, class P2>
    void Read(P1,P2){}

    template<class P1, class P2>
    void Finish(P1,P2){}
};
template<class T1, class T2> class ServerAsyncReaderWriter{
public:
    template<class P1, class P2>
    void Read(P1,P2){}

    template<class P1, class P2>
    void Write(P1,P2) {}
};
int InsecureServerCredentials() { return 0;}
template<class T1, class T2> class ClientAsyncReaderWriter {
public:
    template<class P1, class P2>
    void Read(P1,P2){}

    template<class P1, class P2>
    void Write(P1,P2) {}
};
void EnableDefaultHealthCheckService(bool) {}
namespace reflection {
    void InitProtoReflectionServerBuilderPlugin(){}
}
class Service{};
}
enum {
    GPR_CLOCK_MONOTONIC = 1,
};
struct gpr_timespec {
    int tv_sec;
    int tv_nsec;
    int clock_type;
};
#endif

#endif


