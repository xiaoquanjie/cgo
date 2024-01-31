//
// Created by xiaoqj on 2023/5/26.
//

#include "heap_profiler.h"

#ifdef USE_HEAP_PROFILER
#include <gperftools/heap-profiler.h>
#endif

void HeapProfiler::Switch(const std::string& file_name) {
#ifdef USE_HEAP_PROFILER
    if (IsHeapProfilerRunning()) {
        Stop();
    } else {
        Start(file_name);
    }
#endif
}

void HeapProfiler::Dump() {
#ifdef USE_HEAP_PROFILER
    if (IsHeapProfilerRunning()) {
        HeapProfilerDump("signal");
    }
#endif
}

void HeapProfiler::Start(const std::string& file_name) {
#ifdef USE_HEAP_PROFILER
    if (IsHeapProfilerRunning()) {
        return;
    }

    HeapProfilerStart(file_name.c_str());
#endif
}

void HeapProfiler::Stop() {
#ifdef USE_HEAP_PROFILER
    if (IsHeapProfilerRunning()) {
        HeapProfilerDump("stoped");
        HeapProfilerStop();
    }
#endif
}