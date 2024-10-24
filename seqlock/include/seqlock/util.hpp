#pragma once

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "seqlock.hpp"

namespace seqlock::util {

class SharedMemory {
   private:
    void Create(const std::string& filename, size_t size);

   public:
    SharedMemory() = delete;
    explicit SharedMemory(size_t size);
    explicit SharedMemory(const std::string& filename, size_t size);
    ~SharedMemory() noexcept;

    // Copy.
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Move.
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    void* Ptr() const noexcept { return ptr_; }
    size_t Size() const noexcept { return size_; }
    const std::string& Filename() const noexcept { return filename_; }

    void Close();

    template <typename T, typename... Args>
    T* Map(Args... args) const {
        if (ptr_ == nullptr) {
            throw std::runtime_error("Cannot create in unitialized memory");
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
    std::string filename_;
    size_t size_;
    int fd_{-1};
    void* ptr_{nullptr};
    bool is_creator_{false};

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
