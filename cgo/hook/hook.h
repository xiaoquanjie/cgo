//
// Created by xiaoqj on 2023/12/20.
//

#define CGO_USE_HOOK

#ifdef CGO_USE_HOOK

#ifdef __GNUC__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <unistd.h>
#elif _MSC_VER
#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#define close closesocket
#endif

#undef msleep
#define msleep cgo::hook::mSleep

// hook some fd
#undef cgo_hook_fd
#define cgo_hook_fd cgo::hook::hook_fd

// set global hook flag
#undef cgo_global_hook
#define cgo_global_hook cgo::hook::set_global_hook

#undef CGO_MAX_HOOK_FD
#define CGO_MAX_HOOK_FD 1024*100

namespace cgo {
    namespace hook {
        void mSleep(unsigned int millisecond);

        // hook a fd
        void hook_fd(int fd);

        // default disable global hook
        void set_global_hook(bool hook);
    }
}

#endif