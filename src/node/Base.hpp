#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "../Process.hpp"

namespace node {
class Base {
public:
    Base(Process& process);

    void display_address_offset(uintptr_t address, uintptr_t offset);
    void apply_indentation();

    virtual void display(uintptr_t address, uintptr_t offset, std::byte* mem) = 0;
    virtual size_t size() = 0;
    virtual void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {}

protected:
    static int indentation_level;
    Process& m_process;
};

} // namespace node
