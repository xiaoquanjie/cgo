//
// copy from folly::MicroSpinLock -> https://github.com/facebook/folly/blob/master/folly/synchronization/MicroSpinLock.h
//

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace folly {
    inline void asm_volatile_pause() {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
        ::_mm_pause();
#elif defined(__i386__) || FOLLY_X64 || \
    (defined(__mips_isa_rev) && __mips_isa_rev > 1)
        asm volatile("pause");
#elif FOLLY_AARCH64
        asm volatile("isb");
#elif (defined(__arm__) && !(__ARM_ARCH < 7))
        asm volatile("yield");
#elif FOLLY_PPC64
        asm volatile("or 27,27,27");
#endif
    }

    class Sleeper {
        const std::chrono::nanoseconds delta;
        uint32_t spinCount;

        static constexpr uint32_t kMaxActiveSpin = 4000;

    public:
        static constexpr std::chrono::nanoseconds kMinYieldingSleep =
                std::chrono::microseconds(500);

        constexpr Sleeper() noexcept : delta(kMinYieldingSleep), spinCount(0) {}

        explicit Sleeper(std::chrono::nanoseconds d) noexcept
                : delta(d), spinCount(0) {}

        void wait() noexcept {
            if (spinCount < kMaxActiveSpin) {
                ++spinCount;
                asm_volatile_pause();
            } else {
                /* sleep override */
                std::this_thread::sleep_for(delta);
            }
        }
    };

    struct MicroSpinLock {
        enum { FREE = 0, LOCKED = 1 };
        // lock_ can't be std::atomic<> to preserve POD-ness.
        uint8_t lock_;

        // Initialize this MSL.  It is unnecessary to call this if you
        // zero-initialize the MicroSpinLock.
        void init() noexcept { payload()->store(FREE); }

        void lock() noexcept {
            Sleeper sleeper;
            while (xchg(LOCKED) != FREE) {
                do {
                    sleeper.wait();
                } while (payload()->load(std::memory_order_relaxed) == LOCKED);
            }
            assert(payload()->load() == LOCKED);
        }

        void unlock() noexcept {
            assert(payload()->load() == LOCKED);
            payload()->store(FREE, std::memory_order_release);
        }

    private:
        std::atomic<uint8_t>* payload() noexcept {
            return reinterpret_cast<std::atomic<uint8_t>*>(&this->lock_);
        }

        uint8_t xchg(uint8_t newVal) noexcept {
            return std::atomic_exchange_explicit(
                    payload(), newVal, std::memory_order_acq_rel);
        }
    };
    static_assert(
            std::is_standard_layout<MicroSpinLock>::value &&
            std::is_trivial<MicroSpinLock>::value,
            "MicroSpinLock must be kept a POD type.");
}