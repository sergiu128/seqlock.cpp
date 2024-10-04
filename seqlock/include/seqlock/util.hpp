#pragma once

#include <cstring>
#include <stdexcept>

namespace seqlock::util {

class SharedMemory {
   public:
    SharedMemory() = delete;
    explicit SharedMemory(const char* filename, size_t size);
    ~SharedMemory();

    // Copy.
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Move.
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    void* Ptr() const noexcept { return ptr_; }
    size_t Size() const noexcept { return size_; }

    template <typename T, typename... Args>
    T* Map(Args... args) {
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
    size_t size_;
    int fd_{-1};
    void* ptr_{nullptr};
    bool is_creator_{false};
};

size_t GetFileSize(int fd);

size_t GetPageSize();

}  // namespace seqlock::util
