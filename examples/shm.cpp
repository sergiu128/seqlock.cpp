#include <algorithm>
#include <cstring>
#include <seqlock/seqlock.hpp>
#include <seqlock/util.hpp>
#include <thread>

using namespace seqlock;  // NOLINT

/// Region is an object that can be shared between multiple processes. It holds a buffer guarded by a single-writer
/// sequential lock.
class Region {
   public:
    static constexpr size_t kBufferSize = 1024;

    Region() = default;
    ~Region() = default;

    // Copy.
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;

    // Move.
    Region(Region&&) = delete;
    Region& operator=(Region&&) = delete;

    void Set(int v) {
        lock_.Store([&] { std::memset(data_, v, kBufferSize); });
    }

    void Store(char* from, size_t size) {
        lock_.Store([&] { std::memcpy(data_, from, std::min(size, kBufferSize)); });
    }

    void Load(char* into, size_t size) {
        lock_.Load([&] { std::memcpy(into, data_, std::min(size, kBufferSize)); });
    }

   private:
    SeqLock<mode::SingleWriter> lock_;
    char data_[kBufferSize];
};

int main() {  // NOLINT
    const char* filename = "/shmfiletest42";
    const size_t filesize = util::GetPageSize();

    // Here we use an atomic to tell the reader when the shared memory region is initialized. When the writer and the
    // reader are in separate processes, we can instead use named pipes or message queues.
    std::atomic<int> writer_state{0};  // 0: preparing 1: running 2: done

    using namespace std::chrono_literals;

    std::thread writer{[&] {
        util::SharedMemory shm{filename, filesize,
                               [](const std::exception& e) { std::cerr << "writer error: " << e.what() << std::endl; }};
        auto* region = shm.Map<Region>();
        writer_state = 1;

        for (int i = 0; i < 1'000; i++) {
            region->Set(i & 127);

            std::this_thread::sleep_for(1ms);
        }

        writer_state = 2;
    }};

    std::thread reader{[&] {
        while (writer_state == 0) {
            std::this_thread::sleep_for(1ms);
        }

        // Writer initialized, we can now access the shared memory.
        util::SharedMemory shm{filename, filesize,
                               [](const std::exception& e) { std::cerr << "reader error: " << e.what() << std::endl; }};
        auto* region = shm.Map<Region>();

        bool mask[128];
        memset(mask, 0, 128);

        char data[Region::kBufferSize];
        memset(data, 0, Region::kBufferSize);

        while (writer_state == 1) {
            region->Load(data, Region::kBufferSize);
            for (size_t i = 0; i < Region::kBufferSize - 1; i++) {
                if (data[i] != data[i + 1]) {
                    throw std::runtime_error{"invalid shared memory load"};
                }
            }
            mask[(size_t)(data[0])] = true;  // NOLINT
        }

        bool valid = false;
        for (size_t i = 0; i < 128; i++) {
            valid |= mask[i];
        }
        if (not valid) {
            throw std::runtime_error("no loads happened");
        }
    }};

    writer.join();
    reader.join();

    return 0;
}
