#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SingleWriterSeqLock {
    void* lock;
    void* shm;  // optional
    void* shared_data;
    size_t shared_data_size;
    bool shared;
};

struct SingleWriterSeqLock* seqlock_single_writer_create(char* data, size_t size);
struct SingleWriterSeqLock* seqlock_single_writer_create_shared(const char* filename, size_t size);
void seqlock_single_writer_destroy(struct SingleWriterSeqLock*);
void seqlock_single_writer_load(struct SingleWriterSeqLock* wrapper_lock, char* dst, size_t size);
void seqlock_single_writer_store(struct SingleWriterSeqLock* wrapper_lock, char* src, size_t size);

#ifdef __cplusplus
}
#endif
