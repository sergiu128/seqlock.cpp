#pragma once

#include <atomic>
#include <cstring>
#include <functional>
#include <stdexcept>

#include "seqlock.hpp"

namespace seqlock::util {

/// Creates an memory maps a file in shared memory through `shm_open`. The creator of the file (indicated through
/// `SharedMemory::IsCreator()`) is responsible for unlinking it at destruction time.
class SharedMemory {
   private:
    void Create(std::string_view filename, size_t size);

   public:
    using ErrorHandler = std::function<void(const std::exception&)>;

    SharedMemory() = delete;
    explicit SharedMemory(ErrorHandler);
    explicit SharedMemory(size_t, ErrorHandler);
    explicit SharedMemory(std::string_view, size_t, ErrorHandler);
    ~SharedMemory() noexcept;

    // Copy.
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Move.
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    void* Ptr() const noexcept { return ptr_; }
    size_t Size() const noexcept { return size_; }
    std::string_view Filename() const noexcept { return filename_; }
    bool IsCreator() const noexcept { return is_creator_; }

    void Close();

    /// Constructs the given type with the given arguments in-place in the shared memory. Callers must not `delete` the
    /// returned pointer.
    template <typename T, typename... Args>
    T* Map(Args... args) const {
        if (ptr_ == nullptr) {
            throw std::runtime_error{"Cannot create in unitialized memory"};
        }

        T* obj{nullptr};
        if (is_creator_) {
            obj = new (ptr_) T{args...};
        } else {
            obj = static_cast<T*>(ptr_);
        }
        return obj;
    }

   private:
    ErrorHandler on_error_;
    std::string filename_;
    size_t size_;
    void* ptr_{nullptr};
    std::atomic<bool> is_creator_{false};

    void CloseNoExcept() noexcept;
};

size_t GetFileSize(int fd);
size_t GetPageSize();
size_t RoundToPageSize(size_t);

template <seqlock::mode::Mode ModeT, size_t N>
class Region {
   public:
    Region() = default;
    ~Region() = default;

    // Copy.
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;

    // Copy.
    Region(Region&&) = delete;
    Region& operator=(Region&&) = delete;

    void Store(char* from, size_t size) noexcept {
        lock_.Store([&] { std::memcpy(data_, from, size); });
    }

    void Load(char* into, size_t size) noexcept {
        lock_.Load([&] { std::memcpy(into, data_, size); });
    }

    static constexpr size_t Size() noexcept { return sizeof(Region<ModeT, N>); }

    static Region<ModeT, N>* Create(const SharedMemory& shm) {
        if (shm.Size() < N) {
            throw std::runtime_error("region bigger than shared memory size");
        }
        return shm.Map<Region<ModeT, N>>();
    }

   private:
    SeqLock<ModeT> lock_;
    char data_[N];  // data guarded by the above lock
};

}  // namespace seqlock::util
