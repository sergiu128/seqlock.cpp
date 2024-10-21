#include "seqlock/util.hpp"

#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <system_error>
#include <thread>

namespace seqlock::util {

void SharedMemory::Create(const std::string& filename, size_t size) {
    filename_ = filename;
    size_ = RoundToPageSize(size);

    if (size == 0) {
        throw std::runtime_error{"Size cannot be 0"};
    }

    const char* filename_cstr = filename_.c_str();

    if (const auto filename_size = strlen(filename_cstr); filename_size < 0 or filename_size > NAME_MAX) {
        throw std::runtime_error{"File name must be between (0, 255] characters."};
    }

    if (filename[0] != '/') {
        throw std::runtime_error{"File name must start with /"};
    }

    fd_ = shm_open(filename_cstr, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd_ >= 0) {
        if (ftruncate(fd_, static_cast<off_t>(size)) != 0) {
            CloseNoExcept();
            throw std::system_error{errno, std::generic_category(), "ftruncate"};
        }
        is_creator_ = true;
    } else if (errno == EEXIST) {
        errno = 0;
        fd_ = shm_open(filename_cstr, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd_ < 0) {
            CloseNoExcept();
            throw std::system_error{errno, std::generic_category(), "shm_open"};
        }

        if (GetFileSize(fd_) != size_) {
            CloseNoExcept();
            throw std::runtime_error{"size mismatch"};
        }

        is_creator_ = false;
    } else {
        CloseNoExcept();
        throw std::system_error{errno, std::generic_category(), "shm_open"};
    }

    ptr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        CloseNoExcept();
        throw std::system_error{errno, std::generic_category(), "mmap"};
    }
}

SharedMemory::SharedMemory(size_t size) {
    auto epoch =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());

    std::ostringstream oss;
    oss << "/shm-" << getpid() << "-" << epoch.count();

    Create(oss.str(), size);
}

SharedMemory::SharedMemory(const std::string& filename, size_t size) { Create(filename, size); }

void SharedMemory::Close() {
    if (fd_ >= 0) {
        if (is_creator_) {
            shm_unlink(filename_.c_str());
        }
        if (ptr_ != nullptr) {
            munmap(ptr_, size_);
        }
        close(fd_);

        fd_ = -1;
        ptr_ = nullptr;
    }
}

SharedMemory::~SharedMemory() noexcept { CloseNoExcept(); }

void SharedMemory::CloseNoExcept() noexcept {
    try {
        Close();
    } catch (...) {
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

size_t RoundToPageSize(size_t size) {
    const auto page_size = GetPageSize();
    if (size == 0) {
        return page_size;
    }
    if (const auto remainder = size % page_size; remainder > 0) {
        size += page_size - remainder;
    }
    return size;
}

}  // namespace seqlock::util
