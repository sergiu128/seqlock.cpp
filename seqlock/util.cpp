#include "seqlock/util.hpp"

#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <system_error>
#include <thread>

namespace seqlock::util {

SharedMemory::SharedMemory(const char* filename, size_t size) : size_{size} {
    // TODO(sergiu128) if filename nullptr, then create a unique filename

    if (const auto filename_size = strlen(filename); filename_size <= 0 or filename_size > NAME_MAX) {
        throw std::runtime_error{"File size must be between [0, 255] bytes."};
    }

    if (filename[0] != '/') {
        throw std::runtime_error{"File name must start with /"};
    }

    fd_ = shm_open(filename, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd_ >= 0) {
        if (ftruncate(fd_, static_cast<off_t>(size)) != 0) {
            throw std::system_error{errno, std::generic_category(), "ftruncate"};
        }
        is_creator_ = true;
    } else if (errno == EEXIST) {
        fd_ = shm_open(filename, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd_ < 0) {
            throw std::system_error{errno, std::generic_category(), "shm_open"};
        }

        if (GetFileSize(fd_) != size) {
            throw std::runtime_error{"size mismatch"};
        }
    } else {
        throw std::system_error{errno, std::generic_category(), "shm_open"};
    }

    ptr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        ptr_ = nullptr;
        throw std::system_error{errno, std::generic_category(), "mmap"};
    }

    if (shm_unlink(filename) != 0) {
        throw std::system_error{errno, std::generic_category(), "flock"};
    }
}

SharedMemory::~SharedMemory() {
    if (fd_ >= 0) {
        // TODO(sergiu128) Close should throw if unmap fails and here we just catch it
        munmap(ptr_, size_);
        close(fd_);
        fd_ = -1;
    }
}

size_t GetFileSize(int fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) {
        throw std::runtime_error{"Could not fstat file"};
    }
    return st.st_size;
}

size_t GetPageSize() { return sysconf(_SC_PAGESIZE); }

}  // namespace seqlock::util
