//
// Created by xiaoqj on 2023/12/20.
//

#pragma once

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

/*
 * hook的规则：
 * 当设置了cgo_global_hook时，可以不用设置其他hook
 * 当设置了cgo_hook_fd时，只是某个fd有hook
 * 当设置了cgo_hook_poll_select时，只是接下来调用的poll/select这两个函数有hook，有效范围是调用此函数的协程
 * */

// hook a fd
#undef cgo_hook_fd
#define cgo_hook_fd cgo::hook::hook_fd

// set global hook flag
#undef cgo_global_hook
#define cgo_global_hook cgo::hook::set_global_hook

// hook select/poll function
#undef cgo_hook_poll_select
#define cgo_hook_poll_select cgo::hook::hook_poll_select

namespace cgo {
    namespace hook {
        void mSleep(unsigned int millisecond);

        // hook a fd
        void hook_fd(int fd);

        // default disable global hook
        void set_global_hook(bool hook);

        // hook select/poll function
        void hook_poll_select(bool hook);
    }
}
