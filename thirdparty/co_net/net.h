//
// Created by xiaoqj on 2024/2/19.
//

#pragma once

#ifdef __GNUC__
#include <sys/socket.h>
#elif _MSC_VER
#include <WinSock2.h>
#endif

#include <stdint.h>
#include <string>
#include "cgo/cgo.h"


