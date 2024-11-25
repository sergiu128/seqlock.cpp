#include "seqlock/seqlock.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "seqlock/util.hpp"

using namespace seqlock;  // NOLINT

constexpr size_t kBufferSize = 1024;

TEST(SeqLock, CorrectMode) {
    // basically checking if SFINAE works
    ASSERT_EQ(sizeof(SeqLock<mode::SingleWriter>), 8);
    ASSERT_GT(sizeof(SeqLock<mode::MultiWriter>), sizeof(SeqLock<mode::SingleWriter>));
}

TEST(SeqLock, SingleThread) {
    SeqLock<mode::SingleWriter> lock{};
    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    ASSERT_EQ(lock.Sequence(), 0);
    lock.Store([&] { memset(buf, 1, kBufferSize); });
    ASSERT_EQ(lock.Sequence(), 2);

    lock.TryLoad([&] {
        for (size_t i = 0; i < kBufferSize; i++) {
            ASSERT_EQ(buf[i], 1);
        }
    });
    lock.Load([&] {
        for (size_t i = 0; i < kBufferSize; i++) {
            ASSERT_EQ(buf[i], 1);
        }
    });
    ASSERT_EQ(lock.Sequence(), 2);
}

// Synchronizes calls to std::cout between Readers and Writers, since std::cout is not thread-safe by default.
static inline std::mutex cout_mutex{};

template <mode::Mode ModeT>
class Reader {
   public:
    Reader() = delete;
    explicit Reader(const SeqLock<ModeT>& lock, char* buf, size_t id) : lock_{lock}, buf_{buf}, id_{id} {}
    ~Reader() = default;

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    Reader(Reader&&) noexcept = default;
    Reader& operator=(Reader&&) = delete;

    void Run() {
        cout_mutex.lock();
        std::cout << "reader " << id_ << " starting" << std::endl;
        cout_mutex.unlock();

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

        cout_mutex.lock();
        std::cout << "reader " << id_ << " done successful=" << successful << " failed=" << failed << std::endl;
        cout_mutex.unlock();
    }

   private:
    const SeqLock<ModeT>& lock_;
    char* buf_;
    size_t id_;

    char last_[kBufferSize];
};

template <mode::Mode ModeT>
class Writer {
   public:
    Writer() = delete;
    explicit Writer(SeqLock<ModeT>& lock, char* buf, size_t id = 0) : lock_{lock}, buf_{buf}, id_{id} {}
    ~Writer() = default;

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&&) noexcept = default;
    Writer& operator=(Writer&&) = delete;

    void Run()
        requires std::same_as<ModeT, mode::SingleWriter>
    {
        cout_mutex.lock();
        std::cout << "writer starting" << std::endl;
        cout_mutex.unlock();

        for (int i = 0; i < 1'000'000; i++) {
            ASSERT_TRUE(lock_.Sequence() % 2 == 0);
            const auto seq_before = lock_.Sequence();

            lock_.Store([&] {
                ASSERT_TRUE(lock_.Sequence() % 2 != 0);
                memcpy(last_, buf_, kBufferSize);
                memset(buf_, (last_[0] + 1) & 255, kBufferSize);
            });
            ASSERT_EQ(lock_.Sequence(), seq_before + 2);
        }

        cout_mutex.lock();
        std::cout << "writer done" << std::endl;
        cout_mutex.unlock();
    }

    void Run()
        requires std::same_as<ModeT, mode::MultiWriter>
    {
        cout_mutex.lock();
        std::cout << "writer " << id_ << " starting" << std::endl;
        cout_mutex.unlock();

        for (int i = 0; i < 1'000'000; i++) {
            lock_.Store([&] {
                ASSERT_TRUE(lock_.Sequence() % 2 != 0);
                memcpy(last_, buf_, kBufferSize);
                memset(buf_, (last_[0] + 1) & 255, kBufferSize);
            });
        }

        cout_mutex.lock();
        std::cout << "writer " << id_ << " done" << std::endl;
        cout_mutex.unlock();
    }

   private:
    SeqLock<ModeT>& lock_;
    char* buf_;
    size_t id_;

    char last_[kBufferSize];
};

TEST(SeqLock, MultiThreadSingleWriterSingleReader) {
    SeqLock<mode::SingleWriter> lock{};

    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    Reader<mode::SingleWriter> r{lock, buf, 0};
    std::thread rt{&Reader<mode::SingleWriter>::Run, &r};

    Writer<mode::SingleWriter> w{lock, buf};
    std::thread wt{&Writer<mode::SingleWriter>::Run, &w};

    rt.join();
    wt.join();
}

TEST(SeqLock, MultiThreadSingleWriterMultiReader) {
    SeqLock<mode::SingleWriter> lock{};

    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    std::vector<Reader<mode::SingleWriter>> readers;
    std::vector<std::thread> reader_threads;
    constexpr size_t kReaders = 10;
    readers.reserve(kReaders);
    reader_threads.reserve(kReaders);
    std::cout << "will spawn " << kReaders << " readers" << std::endl;
    for (size_t i = 0; i < kReaders; i++) {
        readers.emplace_back(lock, buf, i);
        reader_threads.emplace_back(&Reader<mode::SingleWriter>::Run, &readers.back());
    }

    Writer<mode::SingleWriter> w{lock, buf};
    std::thread wt{&Writer<mode::SingleWriter>::Run, &w};

    for (auto& rt : reader_threads) {
        rt.join();
    }
    wt.join();
}

TEST(SeqLock, MultiThreadMultiWriterMultiReader) {
    SeqLock<mode::MultiWriter> lock{};

    char buf[kBufferSize];
    memset(buf, 0, kBufferSize);

    std::vector<Reader<mode::MultiWriter>> readers;
    std::vector<std::thread> reader_threads;
    constexpr size_t kReaders = 10;
    readers.reserve(kReaders);
    reader_threads.reserve(kReaders);
    std::cout << "will spawn " << kReaders << " readers" << std::endl;
    for (size_t i = 0; i < kReaders; i++) {
        readers.emplace_back(lock, buf, i);
        reader_threads.emplace_back(&Reader<mode::MultiWriter>::Run, &readers.back());
    }

    std::vector<Writer<mode::MultiWriter>> writers;
    std::vector<std::thread> writer_threads;
    constexpr size_t kWriters = 10;
    writers.reserve(kWriters);
    writer_threads.reserve(kWriters);
    std::cout << "will spawn " << kWriters << " writers" << std::endl;
    for (size_t i = 0; i < kWriters; i++) {
        writers.emplace_back(lock, buf, i);
        writer_threads.emplace_back(&Writer<mode::MultiWriter>::Run, &writers.back());
    }

    for (auto& rt : reader_threads) {
        rt.join();
    }
    for (auto& wt : writer_threads) {
        wt.join();
    }
}

TEST(SeqLock, TwoWritersTryStore) {
    constexpr int kIterations = 10;
    for (int i = 0; i < kIterations; i++) {
        SeqLock<mode::MultiWriter> lock{};

        char buf[kBufferSize];
        memset(buf, 0, kBufferSize);

        auto store_fn = [&] {
            memset(buf, 0, kBufferSize);
            ASSERT_TRUE(lock.Sequence() % 2 != 0);
        };

        constexpr int kSuccess = 1000;

        std::atomic<bool> w1_done{false};
        std::atomic<bool> w2_done{false};

        int successful[2];
        int failed[2];
        std::fill(successful, successful + 2, 0);
        std::fill(failed, failed + 2, 0);

        std::thread w1t{[&] {
            while (successful[0] < kSuccess) {
                if (lock.TryStore(store_fn)) {
                    successful[0]++;
                }
                failed[0]++;

                if (w2_done) {
                    ASSERT_FALSE(lock.WriterStalled());
                }
            }

            ASSERT_GT(failed[0], 0);

            w1_done = true;
        }};

        std::thread w2t{[&] {
            while (successful[1] < kSuccess) {
                if (lock.TryStore(store_fn)) {
                    successful[1]++;
                }
                failed[1]++;

                if (w1_done) {
                    ASSERT_FALSE(lock.WriterStalled());
                }
            }

            ASSERT_GT(failed[1], 0);

            w2_done = true;
        }};

        w1t.join();
        w2t.join();

        ASSERT_FALSE(lock.WriterStalled());

        if (i < kIterations - 1) {
            std::cout << "writer 1 successful=" << successful[0] << " failed=" << failed[0] << std::endl;
            std::cout << "writer 2 successful=" << successful[1] << " failed=" << failed[1] << std::endl;
        }
    }
}

TEST(SeqLock, Shm) {
    using namespace std::chrono_literals;

    std::atomic<int> writer_state{0};  // 0: preparing 1: writing 2: done

    std::thread writer{[&] {
        util::SharedMemory shm{"/sharedfile", 128, [](const std::exception& e) { GTEST_FAIL() << e.what(); }};
        auto* region = util::Region<mode::SingleWriter, 128>::Create(shm);

        writer_state = 1;

        for (int i = 0; i < 1'000; i++) {
            char from[64];
            memset(from, i & 127, sizeof(from));
            region->Store(from, 64);
            std::this_thread::sleep_for(1ms);
        }

        writer_state = 2;
    }};

    std::thread reader{[&] {
        while (writer_state == 0) {
        }

        util::SharedMemory shm{"/sharedfile", 128, [](const std::exception& e) { GTEST_FAIL() << e.what(); }};
        auto* region = util::Region<mode::SingleWriter, 128>::Create(shm);
        bool ok = false;
        while (writer_state == 1) {
            char into[64];
            memset(into, 0, 64);
            region->Load(into, 64);
            for (int i = 0; i < 63; i++) {
                if (into[i] > 0) {
                    ASSERT_EQ(into[i], into[i + 1]);
                    ok = true;
                }
            }
            std::this_thread::sleep_for(2ms);
        }
        ASSERT_TRUE(ok);
    }};

    writer.join();
    reader.join();
}
