#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <variant>

#include "seqlock/spinlock.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#define BARRIER asm volatile("" : : : "memory")
#elif defined(__aarch64__) || defined(_M_ARM64)
#define BARRIER asm volatile("dmb sy" : : : "memory")
#else
#define BARRIER static_assert("Unsupported architecture.")
#endif

namespace seqlock {

namespace mode {
struct SingleWriter {};
struct MultiWriter {};

template <typename T>
concept Mode = std::same_as<T, mode::SingleWriter> or std::same_as<T, mode::MultiWriter>;

}  // namespace mode

/// `SeqLock` is a fast, lock-free and potentially wait-free multi-writer-multi-reader lock that guarantees writers are
/// not starved by readers. This comes at the expense of readers having to retry reads until they're successful. If
/// there is a single writer, all writes are guaranteed to be wait-free. If there are multiple writers, then they're
/// synchronized through a spin-lock which means that writes are not wait-free anymore.
///
/// It is guaranteed that the performance of the `SeqLock` stays the same no matter the number of readers.
///
/// Callers are expected to instantiate the right SeqLock based on the number of writers: `SeqLock<mode::SingleWriter>`
/// for a single writer or `SeqLock<mode::MultiWriter>` for multiple writers. The right `Store` function is chosen at
/// compile-time based on the passed mode. Readers are not impacted by the writer `mode`: the `Load` function is the
/// same no matter the `mode`. For multi-process synchronization (see the `examples` folder), it is recommended that the
/// readers have the same mode as the writers for code clarity.
template <mode::Mode ModeT>
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
    /// function is not called in the `store_fn` function in `Store`.
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

    /// `Store` executes `store_fn`, a function meant to update the shared memory synchronized through this lock. This
    /// function is only defined if the mode is `mode::SingleWriter. It is guaranteed that the single writer that
    /// invokes `Store` is not starved by any of the readers through `Load` or `TryLoad`.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    void Store(StoreFnT&& store_fn) noexcept
        requires std::same_as<ModeT, mode::SingleWriter>
    {
        SingleWriterStore(std::forward<StoreFnT>(store_fn));
    }

    /// `Store` executes `store_fn`, a function meant to update the shared memory synchronized through this lock.
    /// This function is only defined if the mode is `mode::MultiWriter`. The writer that calls `Store` might be
    /// starved by another writer whose write is currently in progress. To have control over this starvation, look at
    /// `TryStore`. It is guaranteed that any of the writers that invoke `Store` are not starved by any of the readers
    /// through `Load` or `TryLoad`.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    void Store(StoreFnT&& store_fn) noexcept
        requires std::same_as<ModeT, mode::MultiWriter>
    {
        writer_lock_([&] { SingleWriterStore(std::forward<StoreFnT>(store_fn)); });
    }

    /// `TryStore` tries to execute `store_fn`, a function meant to update the shared memory synchronized through
    /// this lock. This function is only define if the mode is `mode::MultiWriter`. This function returns `false` if
    /// there is already a write in progress. Otherwise, `store_fn` is executed and `true` is returned.
    ///
    /// Callers must ensure `store_fn` only stores to and does load anything from the shared memory that's synchronized
    /// through the `SeqLock`.
    template <typename StoreFnT>
    bool TryStore(StoreFnT&& store_fn) noexcept
        requires std::same_as<ModeT, mode::MultiWriter>
    {
        if (writer_lock_.TryAcquire()) {
            SingleWriterStore(std::forward<StoreFnT>(store_fn));
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
    void Load(LoadFnT&& load_fn) const noexcept {
        while (not TryLoad(std::forward<LoadFnT>(load_fn))) {
        }
    }

   private:
    alignas(64) SeqT seq_{0};

    // Define the `SpinLock` only if in `mode::MultiWriter`. Otherwise, this member variable occupies 0 bytes. It is
    // guaranteed that `seq_` is when this lock is not held.
    [[no_unique_address]] std::conditional_t<std::is_same_v<ModeT, mode::MultiWriter>, SpinLock, std::monostate>
        writer_lock_{};

    template <typename StoreFnT>
    void SingleWriterStore(StoreFnT&& store_fn) {
        const SeqT::value_type seq_init = seq_.load(std::memory_order::relaxed);
        seq_.store(seq_init + 1, std::memory_order::relaxed);
        BARRIER;
        store_fn();
        seq_.store(seq_init + 2, std::memory_order::release);
    }
};

/// A utility class holding N bytes guarded by a SeqLock of the given mode.
template <mode::Mode ModeT, size_t N>
class GuardedRegion {
   public:
    GuardedRegion() = default;
    ~GuardedRegion() = default;

    // Copy.
    GuardedRegion(const GuardedRegion&) = delete;
    GuardedRegion& operator=(const GuardedRegion&) = delete;

    // Move.
    GuardedRegion(GuardedRegion&&) = delete;
    GuardedRegion& operator=(GuardedRegion&&) = delete;

    void Set(int v) {
        lock_.Store([&] { std::memset(data_, v, N); });
    }

    void Store(char* from, size_t size) {
        lock_.Store([&] { std::memcpy(data_, from, std::min(size, N)); });
    }

    void Load(char* into, size_t size) {
        lock_.Load([&] { std::memcpy(into, data_, std::min(size, N)); });
    }

    static constexpr size_t Size() noexcept { return N; }

   private:
    SeqLock<ModeT> lock_;
    char data_[N];
};

}  // namespace seqlock
