#include "Process.hpp"

bool Process::read(uintptr_t address, void* buffer, size_t size) {
    // If we're reading from read-only memory we can just use the cached version since it hasn't changed.
    for (auto&& ro_allocation : m_read_only_allocations) {
        if (ro_allocation.start <= address && address + size <= ro_allocation.end &&
            ro_allocation.mem.size() == ro_allocation.size) {
            auto offset = address - ro_allocation.start;
            memcpy(buffer, ro_allocation.mem.data(), size);
            return true;
        }
    }

    return handle_read(address, buffer, size);
}
