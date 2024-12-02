#pragma once

#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <limits>

namespace seqlock::util {

inline size_t GetPageSize() { return ::sysconf(_SC_PAGESIZE); }

inline std::expected<size_t, std::string> RoundToPageSize(size_t size) {
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

inline std::expected<size_t, std::string> GetFileSize(int fd) {
    struct stat st {};
    if (::fstat(fd, &st) != 0) {
        return std::unexpected(std::format("Cannot get file size for fd {} err=", fd, std::strerror(errno)));
    }
    return st.st_size;
}

/// Memory maps `T` in the given file. If the file does not exist, it is created and T is construced with the arguments
/// provided to `Create`. If the file exists, then it is opened and T is memory mapped directly from it. The file size
/// is rounded up to the nearest page size and bumped to be >= sizeof(T). After a successful `Create(...)` call, callers
/// can retrieve a `T*` with `Get()`.
///
/// For example:
///
/// ```cpp
/// class Foo {
///  public:
///     Foo(int i) : i_{i} {
///         memset(buf, 0, 4096);
///     }
///     char buf[4096];
/// };
/// auto shm = SharedMemory::Create("/shmobj", sizeof(Foo), )
/// Foo* foo = shm.Get();
/// // assumming a 4KiB page size
/// assert(shm.Size() == 8192 /* two pages */);
/// ```
template <typename T, typename... Args>
class SharedMemory {
   private:
    T* obj_;
    std::string filename_;
    size_t size_;
    bool is_creator_;

    SharedMemory() = delete;  // See `Create(...)`
    SharedMemory(T* obj, const std::string& filename, size_t size, bool is_creator)
        : obj_{obj}, filename_{filename}, size_{size}, is_creator_{is_creator} {}

   public:
    // Copy.
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Move.
    SharedMemory(SharedMemory&& other) noexcept
        : obj_{other.obj_}, filename_{other.filename_}, size_{other.size_}, is_creator_{other.is_creator_} {
        other.obj_ = nullptr;
    }
    SharedMemory& operator=(SharedMemory&&) = delete;

   private:
    static std::expected<T*, std::string> MapNew(int fd, const std::string& filename, size_t size, Args&&... args) {
#if defined(__linux__)
        if (::flock(fd, LOCK_EX) != 0) {
            return std::unexpected(
                std::format("Could not acquire file lock on fd {} err={}.", fd, std::strerror(errno)));
        }
#endif

        if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
            return std::unexpected(
                std::format("Cannot truncate file {} to {} err={}.", filename, size, std::strerror(errno)));
        }

        void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            return std::unexpected(std::format("Cannot mmap file {} err={}.", filename, std::strerror(errno)));
        }

        T* obj = new (ptr) T{std::forward<Args>(args)...};

#if defined(__linux__)
        if (::flock(fd, LOCK_UN) != 0) {
            ::munmap(ptr, size);
            return std::unexpected(
                std::format("Could not release file lock on fd {} err={}.", fd, std::strerror(errno)));
        }
#endif

        return obj;
    }

    static std::expected<T*, std::string> MapExisting(int fd, const std::string& filename, size_t size) {
#if defined(__linux__)
        // TODO(@sergiu128): not really ideal, but on macOS we cannot lock file returned through shm_open. Solution here
        // is to also allow shared memory to be made in non-tmpfs filesystems, although the performance there will be
        // worse as the file is regularly flushed, usually during page eviction.
        if (::flock(fd, LOCK_EX) != 0) {
            return std::unexpected(
                std::format("Could not acquire file lock on fd {} err={}.", fd, std::strerror(errno)));
        }
#endif

        const auto file_size_result = GetFileSize(fd);
        if (not file_size_result) {
            return std::unexpected(std::format("File {} err={}", filename, file_size_result.error()));
        }

        if (const auto actual_size = file_size_result.value(); actual_size != size) {
            return std::unexpected(
                std::format("Size mismatch for file {} actual = {} != {} = expected", filename, actual_size, size));
        }

        void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            return std::unexpected(std::format("Cannot mmap file {} err={}.", filename, std::strerror(errno)));
        }

        T* obj = static_cast<T*>(ptr);

#if defined(__linux__)
        if (::flock(fd, LOCK_UN) != 0) {
            ::munmap(ptr, size);
            return std::unexpected(
                std::format("Could not release file lock on fd {} err={}.", fd, std::strerror(errno)));
        }
#endif

        return obj;
    }

   public:
    /// If the file does not exist, `Create` creates a new memory-shared file of the given size, constructing `T` with
    /// the provided constructor arguments `Args` in the new memory. If the file exists, `Create` just maps it. The
    /// filename size must not exceed `NAME_MAX`, which on most platforms is 255. It must start with a '/'.
    static std::expected<SharedMemory, std::string> Create(const std::string& filename, size_t size, Args&&... args) {
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
        std::expected<T*, std::string> map_result;

        int fd = ::shm_open(filename.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd >= 0) {
            is_creator = true;
            map_result = MapNew(fd, filename, size, std::forward<Args>(args)...);
        } else if (errno == EEXIST) {
            is_creator = false;

            errno = 0;
            fd = ::shm_open(filename.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
            if (fd < 0) {
                return std::unexpected(
                    std::format("Could not shm_open existing file {} err={}.", filename, std::strerror(errno)));
            }

            map_result = MapExisting(fd, filename, size);
        } else {
            return std::unexpected(std::format("Cannot open shared memory file {} of size {} err={}", filename, size,
                                               std::strerror(errno)));
        }

        ::close(fd);

        if (not map_result.has_value()) {
            if (is_creator) {
                ::shm_unlink(filename.c_str());
            }
            return std::unexpected(map_result.error());
        }
        T* obj = map_result.value();

        return SharedMemory{obj, filename, size, is_creator};
    }

    ~SharedMemory() noexcept {
        if (obj_ != nullptr) {
            ::munmap(static_cast<void*>(obj_), size_);
            obj_ = nullptr;

            if (is_creator_) {
                ::shm_unlink(filename_.c_str());
            }
        }
    }

    T* Get() noexcept { return obj_; }
    const T* Get() const noexcept { return obj_; }

    void* GetRaw() noexcept { return static_cast<void*>(obj_); }
    const void* GetRaw() const noexcept { return static_cast<void*>(obj_); }

    size_t Size() const { return size_; }
};

}  // namespace seqlock::util
