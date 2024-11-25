#include <cstdlib>
#include <cstring>
#include <iostream>
#include <seqlock/seqlock.hpp>
#include <seqlock/util.hpp>
#include <thread>

using namespace seqlock;               // NOLINT
using namespace std::chrono_literals;  // NOLINT

int main() {  // NOLINT
    const char* filename = "/shmfiletest42";
    const size_t filesize = util::GetPageSize();

    util::SharedMemory shm{filename, filesize, [](const std::exception&) {}};
    auto* region = util::Region<mode::SingleWriter, 4096>::Create(shm);

    std::atomic<int> writer_state{0};  // 0: preparing 1: working 2: done

    std::thread writer{[&] {
        char from[4096];
        memset(from, 0, 4096);

        writer_state = 1;
        for (int i = 0; i < 1024; i++) {
            memset(from, i & 127, 4096);  // NOLINT
            region->Store(from, 4096);
            std::this_thread::sleep_for(1ms);
        }

        writer_state = 2;
    }};

    std::atomic<bool> ok{false};
    std::thread reader{[&] {
        while (writer_state == 0) {
            std::this_thread::sleep_for(10ms);
        }

        char to[4096];
        memset(to, 0, 4096);
        while (writer_state == 1) {
            region->Load(to, 4096);
            if (to[0] > 0) {
                ok = true;
                for (int i = 0; i < 4095; i++) {
                    if (to[i] != to[i + 1]) {
                        std::cout << "invalid load" << std::endl;
                        std::terminate();
                    }
                }
            }
        }
    }};

    writer.join();
    reader.join();

    if (not ok) {
        std::cout << "invalid load" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "load successful" << std::endl;
    return EXIT_SUCCESS;
}
