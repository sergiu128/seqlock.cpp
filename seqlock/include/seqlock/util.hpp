#pragma once

#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <expected>
#include <format>
#include <limits>

namespace seqlock::util {

size_t GetPageSize() { return ::sysconf(_SC_PAGESIZE); }

std::expected<size_t, std::string> RoundToPageSize(size_t size) {
    const auto page_size = GetPageSize();

    assert(page_size != 0);
    if (page_size == 0) {
        return std::unexpected("Fatal: system's page size is 0");
    }

    if (size == 0) {
        return page_size;
    }

    if (const auto remainder = size % page_size; remainder > 0) {
        if (size > std::numeric_limits<size_t>::max() - (page_size - remainder)) {
            return std::unexpected(std::format("Size {} exceeds allowable limits when rounded to page size.", size));
        }

        size += page_size - remainder;
    }
    return size;
}

std::expected<size_t, std::string> GetFileSize(int fd) {
    struct stat st {};
    if (::fstat(fd, &st) != 0) {
        return std::unexpected(std::format("Cannot get file size for fd {} err=", fd, std::strerror(errno)));
    }
    return st.st_size;
}

/// Memory maps `T` in the given file. If the file does not exist, it is created and T is construced with the arguments
/// provided to the `Factory` in the new file. If the file exists, then it is opened and T is memory mapped directly
/// from it. The file size is rounded up to the nearest page size, and is always >= sizeof(T). After a successful
/// `Factory(...)` call, callers can retrieve a `T*` with `Get()`.
template <typename T>
class SharedMemory {
   private:
    SharedMemory() = delete;
    SharedMemory(T* obj, const std::string& filename, size_t size)
        : obj_{obj}, filename_{filename}, size_{size}, is_creator_{false} {}

   public:
    template <typename... Args>
    static std::expected<SharedMemory, std::string> Factory(const std::string& filename, size_t size, Args&&... args) {
        if (filename.empty() or filename.size() > NAME_MAX) {
            return std::unexpected(std::format("File name {} must be between (0, 255] characters.", filename));
        }
        if (not filename.starts_with("/")) {
            return std::unexpected(std::format("File name {} must start with /.", filename));
        }

        size = std::max(size, sizeof(T));

        const auto page_size_result = RoundToPageSize(size);
        if (not page_size_result) {
            return std::unexpected(page_size_result.error());
        }
        size = page_size_result.value();

        bool is_creator{false};
        T* obj{nullptr};

        int fd = ::shm_open(filename.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd >= 0) {
            is_creator = true;

            if (::flock(fd, LOCK_EX) != 0) {
                return std::unexpected(
                    std::format("Could not acquire file lock on fd {} err={}.", fd, std::strerror(errno)));
            }

            if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
                ::close(fd);
                ::shm_unlink(filename.c_str());
                return std::unexpected(
                    std::format("Cannot truncate file {} to {} err={}.", filename, size, std::strerror(errno)));
            }

            void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            ::close(fd);
            if (ptr == MAP_FAILED) {
                ::shm_unlink(filename.c_str());
                return std::unexpected(std::format("Cannot mmap file {} err={}.", filename, std::strerror(errno)));
            }

            obj = new (ptr) T{std::forward<Args>(args)...};

            if (::flock(fd, LOCK_UN) != 0) {
                return std::unexpected(
                    std::format("Could not release file lock on fd {} err={}.", fd, std::strerror(errno)));
            }

        } else if (errno == EEXIST) {
            is_creator = false;

            errno = 0;
            fd = ::shm_open(filename.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
            if (fd < 0) {
                return std::unexpected(
                    std::format("Could not shm_open existing file {} err={}.", filename, std::strerror(errno)));
            }

            const auto file_size_result = GetFileSize(fd);
            if (not file_size_result) {
                ::close(fd);
                return std::unexpected(std::format("File {} err={}", filename, file_size_result.error()));
            }

            if (const auto actual_size = file_size_result.value(); actual_size != size) {
                ::close(fd);
                return std::unexpected(
                    std::format("Size mismatch for file {} actual = {} != {} = expected", filename, actual_size, size));
            }

            void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            ::close(fd);
            if (ptr == MAP_FAILED) {
                return std::unexpected(std::format("Cannot mmap file {} err={}.", filename, std::strerror(errno)));
            }

            obj = static_cast<T*>(ptr);
        } else {
            return std::unexpected(std::format("Cannot open shared memory file {} of size {} err={}", filename, size,
                                               std::strerror(errno)));
        }

        return SharedMemory{obj, filename, size, is_creator};
    }

    ~SharedMemory() noexcept {
        if (obj_ != nullptr) {
            ::munmap(static_cast<void*>(obj_), size_);
            obj_ = nullptr;
        }
        if (is_creator_) {
            ::shm_unlink(filename_.c_str());
        }
    }

    // Copy.
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Move.
    SharedMemory(SharedMemory&&) = default;
    SharedMemory& operator=(SharedMemory&&) = delete;

    T* Get() noexcept { return obj_; }
    const T* Get() const noexcept { return obj_; }

    void* GetRaw() noexcept { return static_cast<void*>(obj_); }
    const void* GetRaw() const noexcept { return static_cast<void*>(obj_); }

    size_t Size() const { return size_; }

   private:
    T* obj_;
    std::string filename_;
    size_t size_;
    bool is_creator_;
};

}  // namespace seqlock::util
