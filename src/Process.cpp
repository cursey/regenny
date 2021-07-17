#include "Process.hpp"

bool Process::read(uintptr_t address, void* buffer, size_t size) {
    // If we're reading from read-only memory we can just use the cached version since it hasn't changed.
    for (auto&& ro_allocation : m_read_only_allocations) {
        if (ro_allocation.start <= address && address + size <= ro_allocation.end &&
            ro_allocation.mem.size() == ro_allocation.size) {
            auto offset = address - ro_allocation.start;

            // two incase the size causes overflow
            if (offset >= ro_allocation.mem.size() || offset + size >= ro_allocation.mem.size()) {
                return false;
            }

            memcpy(buffer, ro_allocation.mem.data() + offset, size);
            return true;
        }
    }

    return handle_read(address, buffer, size);
}

bool Process::write(uintptr_t address, const void* buffer, size_t size) {
    return handle_write(address, buffer, size);
}

const Process::Module* Process::get_module_within(uintptr_t addr) const {
    for (auto& mod : modules()) {
        if (addr >= mod.start && addr <= mod.end) {
            return &mod;
        }
    }

    return nullptr;
}

