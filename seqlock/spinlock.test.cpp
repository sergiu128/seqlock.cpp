#include "seqlock/spinlock.hpp"

#include <gtest/gtest.h>

using seqlock::SpinLock;

TEST(SpinLock, IsCorrect) {
    SpinLock lock{};

    ASSERT_FALSE(lock.IsAcquired());

    lock.Acquire();
    ASSERT_TRUE(lock.IsAcquired());
    ASSERT_FALSE(lock.TryAcquire());

    lock.Release();
    ASSERT_FALSE(lock.IsAcquired());

    ASSERT_TRUE(lock.TryAcquire());
    ASSERT_TRUE(lock.IsAcquired());

    lock.Release();
    ASSERT_FALSE(lock.IsAcquired());

    lock([&] { ASSERT_TRUE(lock.IsAcquired()); });
    ASSERT_FALSE(lock.IsAcquired());
}
