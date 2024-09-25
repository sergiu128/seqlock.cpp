#pragma once

#include <atomic>

namespace seqlock {

class SpinLock {
   public:
    SpinLock() = default;
    ~SpinLock() = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    SpinLock(SpinLock&&) = delete;
    SpinLock& operator=(SpinLock&&) = delete;

    void Acquire() noexcept {
        while (true) {
            if (not acquired_.test_and_set(std::memory_order_acquire)) {
                break;
            }
            while (acquired_.test(std::memory_order_relaxed)) {
            }
        }
    }

    bool TryAcquire() noexcept {
        return (not IsAcquired()) and (not acquired_.test_and_set(std::memory_order_acquire));
    }

    bool IsAcquired() const noexcept { return acquired_.test(std::memory_order_relaxed); }

    void Release() noexcept { acquired_.clear(std::memory_order_release); }

    template <typename FnT>
    void operator()(FnT&& fn) {
        this->Acquire();
        fn();
        this->Release();
    }

   private:
    std::atomic_flag acquired_{false};
};

}  // namespace seqlock
