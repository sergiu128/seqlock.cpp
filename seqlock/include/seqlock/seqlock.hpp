#pragma once

#include <atomic>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#define BARRIER asm volatile("" : : : "memory")
#elif defined(__aarch64__) || defined(_M_ARM64)
#define BARRIER asm volatile("dmb sy" : : : "memory")
#else
#define BARRIER static_assert("Unsupported architecture.")
#endif

namespace seqlock {

class SeqLock {
   public:
    using SeqT = std::atomic<uint64_t>;

    ~SeqLock() = default;

    SeqLock(const SeqLock&) = delete;
    SeqLock& operator=(const SeqLock&) = delete;

    SeqLock(SeqLock&&) = delete;
    SeqLock& operator=(SeqLock&&) = delete;

    template <typename StoreFnT>
    static void StoreSingle(SeqT& seq, StoreFnT&& store_fn) noexcept {
        const SeqT::value_type seq_init = seq.load(std::memory_order::relaxed);
        seq.store(seq_init + 1, std::memory_order::relaxed);
        BARRIER;
        store_fn();
        seq.store(seq_init + 2, std::memory_order::release);
    }

    template <typename LoadFnT>
    static bool Load(const SeqT& seq, LoadFnT&& load_fn) noexcept {
        if (const SeqT::value_type seq_start = seq.load(std::memory_order_relaxed); (seq_start & 1ULL) == 0ULL) {
            std::atomic_thread_fence(std::memory_order_acquire);
            load_fn();
            BARRIER;
            const SeqT::value_type seq_end = seq.load(std::memory_order_relaxed);
            return seq_start == seq_end;
        }
        return false;
    }

   private:
    SeqLock() { static_assert(SeqT::is_always_lock_free, "Sequence number type must be lock-free."); }
};

}  // namespace seqlock
