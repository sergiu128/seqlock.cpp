#include "seqlock/ffi.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "seqlock/seqlock.hpp"
#include "seqlock/util.hpp"

#ifdef __cplusplus
extern "C" {
#endif

struct SingleWriterSeqLock* seqlock_single_writer_create(char* data, size_t size) {
    struct SingleWriterSeqLock* wrapper_lock{nullptr};
    try {
        wrapper_lock = new struct SingleWriterSeqLock;
        wrapper_lock->lock = new seqlock::SeqLock<seqlock::mode::SingleWriter>;
        wrapper_lock->shm = nullptr;
        wrapper_lock->shared_data = data;
        wrapper_lock->shared_data_size = size;
        wrapper_lock->shared = false;
    } catch (const std::exception& e) {
        std::cerr << "seqlock FFI error in C++: " << __FILE_NAME__ << " err: " << e.what() << std::endl;
        errno = EIO;
    } catch (...) {
        std::cerr << "unknown seqlock FFI error in C++: " << __FILE_NAME__ << std::endl;
        errno = EIO;
    }

    return wrapper_lock;
}

struct SingleWriterSeqLock* seqlock_single_writer_create_shared(const char* filename, size_t size) {
    struct SingleWriterSeqLock* wrapper_lock{nullptr};
    try {
        wrapper_lock = new struct SingleWriterSeqLock;

        const size_t seqlock_size = sizeof(seqlock::SeqLock<seqlock::mode::SingleWriter>);
        size += seqlock_size;
        auto* shm = new seqlock::util::SharedMemory{filename, size};
        auto* lock = shm->Map<seqlock::SeqLock<seqlock::mode::SingleWriter>>();

        wrapper_lock->lock = lock;
        wrapper_lock->shm = shm;
        wrapper_lock->shared_data = (char*)shm->Ptr() + seqlock_size;
        assert(shm->Size() > seqlock_size);
        wrapper_lock->shared_data_size = shm->Size() - seqlock_size;
        wrapper_lock->shared = true;
    } catch (const std::exception& e) {
        std::cerr << "seqlock FFI error in C++: " << __FILE_NAME__ << " err: " << e.what() << std::endl;
        errno = EIO;
    } catch (...) {
        std::cerr << "unknown seqlock FFI error in C++: " << __FILE_NAME__ << std::endl;
        errno = EIO;
    }

    return wrapper_lock;
}

void seqlock_single_writer_destroy(struct SingleWriterSeqLock* wrapper_lock) {
    if (wrapper_lock->shared) {
        delete (seqlock::util::SharedMemory*)(wrapper_lock->shm);  // noexcept
    } else {
        delete (seqlock::SeqLock<seqlock::mode::SingleWriter>*)(wrapper_lock->lock);  // noexcept
    }

    delete wrapper_lock;  // noexcept
}

void seqlock_single_writer_load(struct SingleWriterSeqLock* wrapper_lock, char* dst, size_t size) {
    auto* seqlock = (seqlock::SeqLock<seqlock::mode::SingleWriter>*)(wrapper_lock->lock);
    seqlock->Load(
        [&] { memcpy(dst, wrapper_lock->shared_data, std::min(wrapper_lock->shared_data_size, size)); });  // noexcept
}

void seqlock_single_writer_store(struct SingleWriterSeqLock* wrapper_lock, char* src, size_t size) {
    auto* seqlock = (seqlock::SeqLock<seqlock::mode::SingleWriter>*)(wrapper_lock->lock);
    seqlock->Store(
        [&] { memcpy(wrapper_lock->shared_data, src, std::min(wrapper_lock->shared_data_size, size)); });  // noexcept
}

#ifdef __cplusplus
}
#endif
