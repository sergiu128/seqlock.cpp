#pragma once

#include <atomic>
#include <cstdint>

#include "seqlock/spinlock.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#define BARRIER asm volatile("" : : : "memory")
#elif defined(__aarch64__) || defined(_M_ARM64)
#define BARRIER asm volatile("dmb sy" : : : "memory")
#else
#define BARRIER static_assert("Unsupported architecture.")
#endif

namespace seqlock {

/// `SeqLock` is a fast, lock-free and potentially wait-free multi-writer-multi-reader lock that guarantees writers are
/// not starved by readers. This comes at the expense of readers having to retry reads until they're successful. If
/// there is a single writer, all writes are guaranteed to be wait-free. If there are multiple writers, then they're
/// synchronized through a spin-lock which means that writes are not wait-free anymore.
///
/// It is guaranteed that the performance of the `SeqLock` stays the same no matter the number of readers.
///
/// Callers are expected to use only one of the two write functions to update the shared memory in a synchronized
/// manner: either `StoreSingle` for single-writer workflows or `StoreMulti` for multi-writer workflows.
class SeqLock {
   private:
    using SeqT = std::atomic<uint64_t>;

   public:
    SeqLock() { static_assert(SeqT::is_always_lock_free, "Sequence number type must be lock-free."); }
    ~SeqLock() = default;

    SeqLock(const SeqLock&) = delete;
    SeqLock& operator=(const SeqLock&) = delete;

    SeqLock(SeqLock&&) = delete;
    SeqLock& operator=(SeqLock&&) = delete;

    /// `Sequence` returns the current sequence number. The returned sequence number is guaranteed to be even if this
    /// function is not called in the `store_fn` function in `StoreSingle` or `StoreMulti`.
    SeqT Sequence() const noexcept { return seq_.load(std::memory_order_relaxed); }

    /// `WriteInProgress` returns true if there's a write in progress. This is equivalent to checking if the sequence
    /// number is odd.
    ///
    /// Use with care. Note that when there are multiple concurrent writers, the write might complete by the time this
    /// function returns true.
    bool WriteInProgress() const noexcept { return (seq_.load(std::memory_order_relaxed) & 1) != 0; }

    /// `WriterStalled` returns true if at least one writer is stalled from a set of writers updating the shared memory
    /// with `StoreMulti` or `TryStoreMulti`.
    bool WriterStalled() const noexcept { return writer_lock_.IsAcquired(); }

    /// `StoreSingle` executes `store_fn`, a function meant to update the shared memory synchronized through this lock.
    /// This function should only be used in the case of a single writer. If multiple writers must store data during the
    /// call of `store_fn` to the shared memory synchronized through the `SeqLock`, then use `StoreMulti`. It is
    /// guaranteed that the single writer that invokes `StoreSingle` is not starved by readers that exclusively invoke
    /// either `Load` or `TryLoad`.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    void StoreSingle(StoreFnT&& store_fn) noexcept {
        const SeqT::value_type seq_init = seq_.load(std::memory_order::relaxed);
        seq_.store(seq_init + 1, std::memory_order::relaxed);
        BARRIER;
        store_fn();
        seq_.store(seq_init + 2, std::memory_order::release);
    }

    /// `StoreMulti` executes `store_fn`, a function meant to update the shared memory synchronized through this lock.
    /// This function should only be used in the case of multiple writers. The writer that calls `StoreMulti` might be
    /// starved by another writer whose write is currently in progress. To have control over this starvation, look at
    /// `TryStoreMulti`.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    void StoreMulti(StoreFnT&& store_fn) noexcept {
        writer_lock_([&] { StoreSingle(std::forward<StoreFnT>(store_fn)); });
    }

    /// `TryStoreMulti` tries to execute `store_fn`, a function meant to update the shared memory synchronized through
    /// this lock. This function should only be used in the case of multiple writers. This function returns false if
    /// there is already a write in progress. Otherwise, `store_fn` is executed and true is returned.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    bool TryStoreMulti(StoreFnT&& store_fn) noexcept {
        if (writer_lock_.TryAcquire()) {
            StoreSingle(std::forward<StoreFnT>(store_fn));
            writer_lock_.Release();
            return true;
        }
        return false;
    }

    /// `TryLoad` tries to execute the provided `load_fn`, a function meant to read from the shared memory synchronized
    /// through this lock. If the function is executed successfully, `true` is returned - the shared piece of data was
    /// read correctly, in a synchronized manner. Otherwise, `false` is returned.
    ///
    /// Callers must ensure `load_fn` only loads from and does not store anything to the shared memory that's
    /// synchronized through the `SeqLock`.
    template <typename LoadFnT>
    bool TryLoad(LoadFnT&& load_fn) const noexcept {
        if (const SeqT::value_type seq_start = seq_.load(std::memory_order_relaxed); (seq_start & 1ULL) == 0ULL) {
            std::atomic_thread_fence(std::memory_order_acquire);
            load_fn();
            BARRIER;
            const SeqT::value_type seq_end = seq_.load(std::memory_order_relaxed);
            return seq_start == seq_end;
        }
        return false;
    }

    /// `Load` is like `TryLoad` but returns only when `load_fn` executes successfully.
    template <typename LoadFnT>
    void Load(const SeqT& seq, LoadFnT&& load_fn) const noexcept {
        while (not TryLoad(seq, std::forward<LoadFnT>(load_fn))) {
        }
    }

   private:
    SeqT seq_{0};

    // It is guaranteed that `seq_` is even when this lock is not held.
    SpinLock writer_lock_{};
};

}  // namespace seqlock
