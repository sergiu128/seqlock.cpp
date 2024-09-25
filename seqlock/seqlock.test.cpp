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
    auto seq = SeqLock::SeqT{0};
    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    SeqLock::StoreSingle(seq, [&] { memset(buf, 1, kBufferSize); });
    SeqLock::TryLoad(seq, [&] {
        for (size_t i = 0; i < kBufferSize; i++) {
            ASSERT_EQ(buf[i], 1);
        }
    });
}

class Reader {
   public:
    Reader() = delete;
    explicit Reader(const SeqLock::SeqT& seq, char* buf, size_t id) : seq_{seq}, buf_{buf}, id_{id} {}
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
            bool success = SeqLock::TryLoad(seq_, [&] { memcpy(last_, buf_, kBufferSize); });
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
    const SeqLock::SeqT& seq_;
    char* buf_;
    size_t id_;

    char last_[kBufferSize];
    static inline std::mutex cout_mutex_{};
};

class Writer {
   public:
    Writer() = delete;
    explicit Writer(SeqLock::SeqT& seq, char* buf) : seq_{seq}, buf_{buf} {}
    ~Writer() = default;

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&&) = default;
    Writer& operator=(Writer&&) = delete;

    void Run() {
        std::cout << "writer starting" << std::endl;

        for (int i = 0; i < 1'000'000; i++) {
            SeqLock::StoreSingle(seq_, [&] {
                memcpy(last_, buf_, kBufferSize);
                memset(buf_, (last_[0] + 1) & 255, kBufferSize);
            });
        }

        std::cout << "writer done" << std::endl;
    }

   private:
    SeqLock::SeqT& seq_;
    char* buf_;

    char last_[kBufferSize];
};

TEST(SeqLock, MultiThreadSingleWriterSingleReader) {
    SeqLock::SeqT seq{0};
    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    Reader r{seq, buf, 0};
    std::thread rt{&Reader::Run, &r};

    Writer w{seq, buf};
    std::thread wt{&Writer::Run, &w};

    rt.join();
    wt.join();
}

TEST(SeqLock, MultiThreadSingleWriterMultiReader) {
    SeqLock::SeqT seq{0};
    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    std::vector<Reader> readers;
    std::vector<std::thread> reader_threads;
    readers.reserve(10);
    reader_threads.reserve(10);
    for (size_t i = 0; i < 10; i++) {
        readers.emplace_back(seq, buf, i);
        reader_threads.emplace_back(&Reader::Run, &readers.back());
    }

    Writer w{seq, buf};
    std::thread wt{&Writer::Run, &w};

    for (auto& rt : reader_threads) {
        rt.join();
    }
    wt.join();
}
