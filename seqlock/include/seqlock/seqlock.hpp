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

    /// `StoreSingle` executes `store_fn` while updating the provided sequence number. This function should only be used
    /// in the case of a single writer. If multiple writers must store data during the call of `store_fn` to the shared
    /// memory synchronized through the `SeqLock`, then use `StoreMulti`. It is guaranteed that the single writer that
    /// invokes `StoreSingle` is not starved by readers that exclusively invoke either `Load` or `TryLoad`. It is
    /// guaranteed that `seq` is even before and after this call.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    static void StoreSingle(SeqT& seq, StoreFnT&& store_fn) noexcept {
        const SeqT::value_type seq_init = seq.load(std::memory_order::relaxed);
        seq.store(seq_init + 1, std::memory_order::relaxed);
        BARRIER;
        store_fn();
        seq.store(seq_init + 2, std::memory_order::release);
    }

    /// `TryLoad` tries to execute the provided `load_fn` function in-between updates to the given sequence number. If
    /// the function is executed successfully, `true` is returned. This means the sequence number was even at the time
    /// of the function call and maintained its value throughout the function's execution. Otherwise, `false` is
    /// returned - either the sequence number was not even to begin with or a store happened during the execution of
    /// `load_fn`.
    ///
    /// Callers must ensure `load_fn` only loads from and does not store anything to the shared memory that's
    /// synchronized through the `SeqLock`.
    template <typename LoadFnT>
    static bool TryLoad(const SeqT& seq, LoadFnT&& load_fn) noexcept {
        if (const SeqT::value_type seq_start = seq.load(std::memory_order_relaxed); (seq_start & 1ULL) == 0ULL) {
            std::atomic_thread_fence(std::memory_order_acquire);
            load_fn();
            BARRIER;
            const SeqT::value_type seq_end = seq.load(std::memory_order_relaxed);
            return seq_start == seq_end;
        }
        return false;
    }

    /// `Load` is like `TryLoad` but returns only when `load_fn` executes successfully.
    template <typename LoadFnT>
    static void Load(const SeqT& seq, LoadFnT&& load_fn) noexcept {
        while (not TryLoad(seq, std::forward<LoadFnT>(load_fn))) {
        }
    }

   private:
    SeqLock() { static_assert(SeqT::is_always_lock_free, "Sequence number type must be lock-free."); }
};

}  // namespace seqlock
