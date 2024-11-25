#include "seqlock/util.hpp"

#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace seqlock::util {

void SharedMemory::Create(const std::string& filename, size_t size) {
    if (filename.empty() or filename.size() > NAME_MAX) {
        throw std::runtime_error{"File name must be between (0, 255] characters."};
    }
    if (not filename.starts_with("/")) {
        throw std::runtime_error{"File name must start with /."};
    }
    if (std::filesystem::exists(filename)) {
        throw std::runtime_error{"File name exists."};
    }
    filename_ = filename;

    size_ = RoundToPageSize(size);
    if (size == 0) {
        throw std::runtime_error{"Size cannot be 0"};
    }

    auto close_fd = [](int fd) {
        if (fd >= 0) {
            if (::close(fd) != 0) {
                throw std::system_error{errno, std::generic_category(), "close"};
            }
        }
    };

    int fd = ::shm_open(filename_.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd >= 0) {
        if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
            close_fd(fd);
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
            close_fd(fd);
            CloseNoExcept();
            throw std::runtime_error{"size mismatch"};
        }

        is_creator_ = false;
    } else {
        CloseNoExcept();
        throw std::system_error{errno, std::generic_category(), "shm_open"};
    }

    ptr_ = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close_fd(fd);
    if (ptr_ == MAP_FAILED) {
        CloseNoExcept();
        throw std::system_error{errno, std::generic_category(), "mmap"};
    }
}

SharedMemory::SharedMemory(ErrorHandler on_error) : on_error_{std::move(on_error)} {}

SharedMemory::SharedMemory(size_t size, ErrorHandler on_error) : SharedMemory{std::move(on_error)} {
    auto epoch =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());

    std::ostringstream oss;
    oss << "/shm-" << getpid() << "-" << epoch.count();

    Create(oss.str(), size);
}

SharedMemory::SharedMemory(const std::string& filename, size_t size, ErrorHandler on_error)
    : SharedMemory{std::move(on_error)} {
    Create(filename, size);
}

void SharedMemory::Close() {
    if (ptr_ != nullptr) {
        if (::munmap(ptr_, size_) != 0) {
            throw std::system_error{errno, std::generic_category(), "munmap"};
        }
        ptr_ = nullptr;
    }
    if (is_creator_) {
        if (::shm_unlink(filename_.c_str()) != 0) {
            throw std::system_error{errno, std::generic_category(), "shm_unlink"};
        }
    }
}

SharedMemory::~SharedMemory() noexcept { CloseNoExcept(); }

void SharedMemory::CloseNoExcept() noexcept {
    try {
        Close();
    } catch (const std::exception& e) {
        on_error_(e);
    } catch (...) {
        on_error_(std::runtime_error{"unknown exception"});
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
