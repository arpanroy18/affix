#pragma once

#include "types.hpp"
#include <cstddef>
#include <string>
#include <memory>
#include <new>

namespace affix::numa {

[[nodiscard]] bool bind_memory(void* ptr, size_t size_bytes, NumaId node, std::string* error_msg = nullptr);
bool set_thread_preferred(NumaId node, std::string* error_msg = nullptr);
[[nodiscard]] void* allocate(size_t size_bytes, NumaId node);
void free(void* ptr, size_t size_bytes);

template <typename T>
class NumaAllocator {
public:
    template <typename U>
    using rebind = NumaAllocator<U>;

    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    explicit NumaAllocator(NumaId node = 0) noexcept : numa_node_(node) {}

    template <typename U>
    NumaAllocator(const NumaAllocator<U>& other) noexcept : numa_node_(other.numa_node()) {}

    T* allocate(std::size_t n) {
        if (n == 0) return nullptr;
        if (n > std::size_t(-1) / sizeof(T)) throw std::bad_array_new_length();

        size_t bytes = n * sizeof(T);
        void* p = numa::allocate(bytes, numa_node_);
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (p) {
            numa::free(p, n * sizeof(T));
        }
    }

    [[nodiscard]] NumaId numa_node() const noexcept { return numa_node_; }

    template <typename U>
    bool operator==(const NumaAllocator<U>& other) const noexcept {
        return numa_node_ == other.numa_node();
    }

    template <typename U>
    bool operator!=(const NumaAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    NumaId numa_node_ = 0;
};

} // namespace affix::numa
