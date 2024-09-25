#include "seqlock/seqlock.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using seqlock::SeqLock;

constexpr size_t kBufferSize = 1024;

TEST(SeqLock, SingleThread) {
    SeqLock lock{};
    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    ASSERT_EQ(lock.Sequence(), 0);
    lock.StoreSingle([&] { memset(buf, 1, kBufferSize); });
    ASSERT_EQ(lock.Sequence(), 2);

    lock.TryLoad([&] {
        for (size_t i = 0; i < kBufferSize; i++) {
            ASSERT_EQ(buf[i], 1);
        }
    });
    ASSERT_EQ(lock.Sequence(), 2);
}

class Reader {
   public:
    Reader() = delete;
    explicit Reader(const SeqLock& lock, char* buf, size_t id) : lock_{lock}, buf_{buf}, id_{id} {}
    ~Reader() = default;

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    Reader(Reader&&) = default;
    Reader& operator=(Reader&&) = delete;

    void Run() {
        cout_mutex_.lock();
        std::cout << "reader " << id_ << " starting" << std::endl;
        cout_mutex_.unlock();

        int successful = 0;
        int failed = 0;
        while (successful < 100) {
            bool success = lock_.TryLoad([&] { memcpy(last_, buf_, kBufferSize); });
            if (success) {
                for (size_t i = 0; i < kBufferSize - 1; i++) {
                    ASSERT_EQ(last_[i], last_[i + 1]);
                }
                successful++;
            } else {
                failed++;
            }
        }

        cout_mutex_.lock();
        std::cout << "reader " << id_ << " done successful=" << successful << " failed=" << failed << std::endl;
        cout_mutex_.unlock();
    }

   private:
    const SeqLock& lock_;
    char* buf_;
    size_t id_;

    char last_[kBufferSize];

    // Synchronizes calls to std::cout since it's not thread-safe by default.
    static inline std::mutex cout_mutex_{};
};

class Writer {
   public:
    Writer() = delete;
    explicit Writer(SeqLock& lock, char* buf) : lock_{lock}, buf_{buf} {}
    ~Writer() = default;

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&&) = default;
    Writer& operator=(Writer&&) = delete;

    void Run() {
        std::cout << "writer starting" << std::endl;

        for (int i = 0; i < 1'000'000; i++) {
            ASSERT_TRUE(lock_.Sequence() % 2 == 0);
            const auto seq_before = lock_.Sequence();

            lock_.StoreSingle([&] {
                memcpy(last_, buf_, kBufferSize);
                memset(buf_, (last_[0] + 1) & 255, kBufferSize);
            });
            ASSERT_EQ(lock_.Sequence(), seq_before + 2);
        }

        std::cout << "writer done" << std::endl;
    }

   private:
    SeqLock& lock_;
    char* buf_;

    char last_[kBufferSize];
};

TEST(SeqLock, MultiThreadSingleWriterSingleReader) {
    SeqLock lock{};

    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    Reader r{lock, buf, 0};
    std::thread rt{&Reader::Run, &r};

    Writer w{lock, buf};
    std::thread wt{&Writer::Run, &w};

    rt.join();
    wt.join();
}

TEST(SeqLock, MultiThreadSingleWriterMultiReader) {
    SeqLock lock{};

    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    std::vector<Reader> readers;
    std::vector<std::thread> reader_threads;
    constexpr size_t kReaders = 10;
    readers.reserve(kReaders);
    reader_threads.reserve(kReaders);
    std::cout << "will spawn " << kReaders << " readers" << std::endl;
    for (size_t i = 0; i < kReaders; i++) {
        readers.emplace_back(lock, buf, i);
        reader_threads.emplace_back(&Reader::Run, &readers.back());
    }

    Writer w{lock, buf};
    std::thread wt{&Writer::Run, &w};

    for (auto& rt : reader_threads) {
        rt.join();
    }
    wt.join();
}
