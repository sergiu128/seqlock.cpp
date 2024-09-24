#include "seqlock/seqlock.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <thread>

using seqlock::SeqLock;

TEST(SeqLock, SingleThread) {
    auto seq = SeqLock::SeqT{0};
    char buf[1024];
    memset(buf, 0, 1024);

    SeqLock::StoreSingle(seq, [&] { memset(buf, 1, 1024); });
    SeqLock::Load(seq, [&] {
        for (size_t i = 0; i < 1024; i++) {
            ASSERT_EQ(buf[i], 1);
        }
    });
}
