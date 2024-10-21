#include "seqlock/ffi.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "seqlock/seqlock.hpp"
#include "seqlock/util.hpp"

using namespace std::chrono_literals;

constexpr size_t kBufferSize = 4096;

TEST(FFI, SingleWriter) {
    char shared_data[kBufferSize];
    memset(shared_data, 0, kBufferSize);

    auto* lock = seqlock_single_writer_create(shared_data, kBufferSize);

    ASSERT_EQ(lock->shared_data_size, kBufferSize);

    bool mask[128];
    memset(mask, 0, 128);
    std::atomic_flag writer_done{};

    std::thread reader{[&] {
        char reader_data[kBufferSize];
        while (not writer_done.test()) {
            seqlock_single_writer_load(lock, reader_data, kBufferSize);
            for (size_t i = 0; i < kBufferSize - 1; i++) {
                ASSERT_EQ(reader_data[i], reader_data[i + 1]);
            }
            mask[(size_t)reader_data[0]] = true;  // NOLINT
        }
    }};

    std::thread writer{[&] {
        char writer_data[kBufferSize];
        for (int i = 0; i < 100; i++) {
            memset(writer_data, i & 127, kBufferSize);
            seqlock_single_writer_store(lock, writer_data, kBufferSize);
            std::this_thread::sleep_for(10ms);
        }
        writer_done.test_and_set();
    }};

    reader.join();
    writer.join();

    bool at_least_one = false;
    for (size_t i = 0; i < 128; i++) {
        at_least_one |= mask[i];
    }
    ASSERT_TRUE(at_least_one);

    seqlock_single_writer_destroy(lock);
}

TEST(FFI, SingleWriterSharedSize) {
    const auto page_size = seqlock::util::GetPageSize();
    ASSERT_GT(page_size, kBufferSize);

    auto* lock = seqlock_single_writer_create_shared("/somethingshared", page_size);
    ASSERT_EQ(lock->shared_data_size, page_size * 2 - sizeof(seqlock::SeqLock<seqlock::mode::SingleWriter>));

    seqlock_single_writer_destroy(lock);
}

TEST(FFI, SingleWriterShared) {
    std::atomic<int> writer_state = 0;  // 0: preparing 1: storing 2: done
    std::atomic_flag reader_done;
    reader_done.clear();

    std::thread reader{[&] {
        while (writer_state == 0) {
        }

        auto* lock = seqlock_single_writer_create_shared("/yesyesyes2", seqlock::util::GetPageSize());
        char* buf = new char[lock->shared_data_size];
        memset(buf, 0, lock->shared_data_size);

        bool mask[128];
        memset(mask, 0, 128);

        while (writer_state == 1) {
            seqlock_single_writer_load(lock, buf, lock->shared_data_size);
            for (size_t i = 0; i < lock->shared_data_size - 1; i++) {
                ASSERT_EQ(buf[i], buf[i + 1]);
            }
            mask[(size_t)buf[0]] = true;  // NOLINT
        }

        int count{};
        for (size_t i = 0; i < 128; i++) {
            if (mask[i]) {
                count++;
            }
        }
        ASSERT_GT(count, 5);

        delete[] buf;
        seqlock_single_writer_destroy(lock);

        reader_done.test_and_set();
    }};

    std::thread writer{[&] {
        auto* lock = seqlock_single_writer_create_shared("/yesyesyes2", seqlock::util::GetPageSize());
        char* buf = new char[lock->shared_data_size];
        memset(buf, 0, lock->shared_data_size);

        writer_state = 1;

        for (char i = 0; i < 100; i++) {
            for (size_t j = 0; j < lock->shared_data_size; j++) {
                buf[j] = i;
            }
            seqlock_single_writer_store(lock, buf, lock->shared_data_size);
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        writer_state = 2;

        while (not reader_done.test()) {
        }

        delete[] buf;
        seqlock_single_writer_destroy(lock);
    }};

    reader.join();
    writer.join();
}
