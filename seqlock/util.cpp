#include "seqlock/util.hpp"

#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace seqlock::util {

void SharedMemory::CloseFd(int fd) noexcept {
    if (fd >= 0) {
        if (::close(fd) != 0) {
            std::cerr << "could not close file descriptor, error: " << std::strerror(errno) << std::endl;
        }
    }
}

void SharedMemory::Create(const std::string& filename, size_t size) {
    if (filename.empty() or filename.size() > NAME_MAX) {
        throw std::runtime_error{"File name must be between (0, 255] characters."};
    }
    if (not filename.starts_with("/")) {
        throw std::runtime_error{"File name must start with /."};
    }
    filename_ = filename;

    size_ = RoundToPageSize(size);
    if (size == 0) {
        throw std::runtime_error{"Size cannot be 0"};
    }

    int fd = ::shm_open(filename_.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd >= 0) {
        if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
            CloseFd(fd);
            CloseNoExcept();
            throw std::system_error{errno, std::generic_category(), "ftruncate"};
        }
        is_creator_ = true;
    } else if (errno == EEXIST) {
        errno = 0;
        fd = ::shm_open(filename_.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd < 0) {
            CloseNoExcept();
            throw std::system_error{errno, std::generic_category(), "shm_open"};
        }

        if (GetFileSize(fd) != size_) {
            CloseFd(fd);
            CloseNoExcept();
            throw std::runtime_error{"size mismatch"};
        }

        is_creator_ = false;
    } else {
        CloseNoExcept();
        throw std::system_error{errno, std::generic_category(), "shm_open"};
    }

    ptr_ = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CloseFd(fd);
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
    if (is_creator_) {
        ::shm_unlink(filename_.c_str());
    }
    if (ptr_ != nullptr) {
        ::munmap(ptr_, size_);
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
    if (::fstat(fd, &st) != 0) {
        throw std::runtime_error{"Could not fstat file."};
    }
    return st.st_size;
}

size_t GetPageSize() { return ::sysconf(_SC_PAGESIZE); }

size_t RoundToPageSize(size_t size) {
    const auto page_size = GetPageSize();
    if (size == 0) {
        return page_size;
    }

    if (const auto remainder = size % page_size; remainder > 0) {
        if (size > std::numeric_limits<size_t>::max() - (page_size - remainder)) {
            throw std::runtime_error{"Size exceeds allowable limits when rounded to page size."};
        }

        size += page_size - remainder;
    }
    return size;
}

}  // namespace seqlock::util
