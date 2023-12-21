//
// Created by xiaoqj on 2023/12/20.
//

#define CGO_USE_HOOK

#ifdef CGO_USE_HOOK

#ifdef __GNUC__
#include <sys/socket.h>
#include <dlfcn.h>
#include <unistd.h>
#elif _MSC_VER
#include <WinSock2.h>
#include <windows.h>
#endif

#undef msleep
#define msleep cgo::hook::mSleep

#undef cgo_enable_hook
#define cgo_enable_hook cgo::hook::enable_hook

namespace cgo {
    namespace hook {
        void mSleep(unsigned int millisecond);

        void enable_hook();
    }
}

#endif