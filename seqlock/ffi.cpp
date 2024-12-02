#include "seqlock/ffi.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

#include "seqlock/seqlock.hpp"
#include "seqlock/util.hpp"

#ifdef __cplusplus
extern "C" {
#endif

struct SingleWriterSeqLock* seqlock_single_writer_create(char* data, size_t size) {
    auto* wrapper_lock = new struct SingleWriterSeqLock;
    wrapper_lock->lock = new seqlock::SeqLock<seqlock::mode::SingleWriter>;
    wrapper_lock->shm = nullptr;
    wrapper_lock->shared_data = data;
    wrapper_lock->shared_data_size = size;
    wrapper_lock->shared = false;

    return wrapper_lock;
}

struct SingleWriterSeqLock* seqlock_single_writer_create_shared(const char* filename, size_t size) {
    using T = seqlock::SeqLock<seqlock::mode::SingleWriter>;

    const size_t seqlock_size = sizeof(T);
    size += seqlock_size;

    auto shm_result = seqlock::util::SharedMemory<T>::Create(std::string{filename}, size);
    if (not shm_result) {
        std::cout << shm_result.error() << std::endl;
        return nullptr;
    }
    auto* shm = new seqlock::util::SharedMemory<T>(std::move(shm_result.value()));

    auto* wrapper_lock = new struct SingleWriterSeqLock;
    wrapper_lock->lock = shm->GetRaw();
    wrapper_lock->shm = static_cast<void*>(shm);
    wrapper_lock->shared_data = static_cast<void*>(static_cast<std::byte*>(shm->GetRaw()) + seqlock_size);
    wrapper_lock->shared_data_size = shm->Size() - seqlock_size;
    wrapper_lock->shared = true;

    return wrapper_lock;
}

void seqlock_single_writer_destroy(struct SingleWriterSeqLock* wrapper_lock) {
    using T = seqlock::SeqLock<seqlock::mode::SingleWriter>;

    if (wrapper_lock->shared) {
        delete (seqlock::util::SharedMemory<T>*)(wrapper_lock->shm);
    } else {
        delete (seqlock::SeqLock<seqlock::mode::SingleWriter>*)(wrapper_lock->lock);
    }

    delete wrapper_lock;
}

void seqlock_single_writer_load(struct SingleWriterSeqLock* wrapper_lock, char* dst, size_t size) {
    assert(wrapper_lock != nullptr);
    assert(wrapper_lock->lock != nullptr);
    assert(wrapper_lock->shared_data != nullptr);
    auto* seqlock = static_cast<seqlock::SeqLock<seqlock::mode::SingleWriter>*>(wrapper_lock->lock);
    seqlock->Load([&] { ::memcpy(dst, wrapper_lock->shared_data, std::min(wrapper_lock->shared_data_size, size)); });
}

void seqlock_single_writer_store(struct SingleWriterSeqLock* wrapper_lock, char* src, size_t size) {
    assert(wrapper_lock != nullptr);
    assert(wrapper_lock->lock != nullptr);
    assert(wrapper_lock->shared_data != nullptr);
    auto* seqlock = static_cast<seqlock::SeqLock<seqlock::mode::SingleWriter>*>(wrapper_lock->lock);
    seqlock->Store([&] { ::memcpy(wrapper_lock->shared_data, src, std::min(wrapper_lock->shared_data_size, size)); });
}

void seqlock_single_writer_assign(struct SingleWriterSeqLock* wrapper_lock, int value) {
    assert(wrapper_lock != nullptr);
    assert(wrapper_lock->lock != nullptr);
    assert(wrapper_lock->shared_data != nullptr);
    auto* seqlock = static_cast<seqlock::SeqLock<seqlock::mode::SingleWriter>*>(wrapper_lock->lock);
    seqlock->Store([&] { ::memset(wrapper_lock->shared_data, value, wrapper_lock->shared_data_size); });
}

#ifdef __cplusplus
}
#endif
