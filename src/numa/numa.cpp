#include "affix/numa.hpp"
#include "../platform/platform.hpp"

namespace affix::numa {

bool bind_memory(void* ptr, size_t size_bytes, NumaId node, std::string* error_msg) {
    return internal::platform_bind_memory_to_numa(ptr, size_bytes, node, error_msg);
}

bool set_thread_preferred(NumaId node, std::string* error_msg) {
    return internal::platform_set_thread_preferred_numa(node, error_msg);
}

void* allocate(size_t size_bytes, NumaId node) {
    return internal::platform_allocate_numa(size_bytes, node);
}

void free(void* ptr, size_t size_bytes) {
    internal::platform_free_numa(ptr, size_bytes);
}

} // namespace affix::numa
