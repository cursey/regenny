#pragma once

#include <cstddef>
#include <cstdint>

namespace node {
class Base {
public:
    void display_address_offset(uintptr_t address, uintptr_t offset);
    virtual void display(uintptr_t address, uintptr_t offset, std::byte* mem) = 0;
    virtual size_t size() = 0;

protected:
    static int indentation_level;
};
} // namespace node
