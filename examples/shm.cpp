#include <algorithm>
#include <cstring>
#include <iostream>
#include <seqlock/seqlock.hpp>
#include <seqlock/util.hpp>
#include <thread>

using namespace seqlock;  // NOLINT

using Region = seqlock::GuardedRegion<seqlock::mode::SingleWriter, 1024>;

int main() {  // NOLINT
    const char* filename = "/shmfiletest42";
    const size_t filesize = util::GetPageSize();

    // Here we use an atomic to tell the reader when the shared memory region is initialized. When the writer and the
    // reader are in separate processes, we can instead use named pipes or message queues.
    std::atomic<int> writer_state{0};  // 0: preparing 1: running 2: done

    using namespace std::chrono_literals;

    std::thread writer{[&] {
        auto shm = util::SharedMemory<Region>::Create(filename, filesize);
        if (not shm) {
            std::cout << "writer could not map memory err=" << shm.error() << std::endl;
            return;
        }
        auto* region = shm->Get();

        std::cout << "writer mapped memory of size " << shm->Size() << std::endl;

        writer_state = 1;

        for (int i = 0; i < 1'000; i++) {
            region->Set(i & 127);

            std::this_thread::sleep_for(1ms);
        }

        writer_state = 2;

        std::cout << "writer done" << std::endl;
    }};

    std::thread reader{[&] {
        while (writer_state == 0) {
            std::this_thread::sleep_for(1ms);
        }

        auto shm = util::SharedMemory<Region>::Create(filename, filesize);
        if (not shm) {
            std::cout << "reader could not map memory err=" << shm.error() << std::endl;
        }
        auto* region = shm->Get();

        std::cout << "reader mapped memory of size " << shm->Size() << std::endl;

        bool mask[128];
        memset(mask, 0, 128);

        char data[Region::Size()];
        memset(data, 0, sizeof(data));

        while (writer_state == 1) {
            region->Load(data, sizeof(data));
            for (size_t i = 0; i < sizeof(data) - 1; i++) {
                if (data[i] != data[i + 1]) {
                    std::cout << "invalid shared memory load" << std::endl;
                    return;
                }
            }
            mask[(size_t)(data[0])] = true;  // NOLINT
        }

        bool valid = false;
        for (size_t i = 0; i < 128; i++) {
            valid |= mask[i];
        }
        if (not valid) {
            std::cout << "no loads happened" << std::endl;
        }

        std::cout << "reader done" << std::endl;
    }};

    writer.join();
    reader.join();

    return 0;
}
