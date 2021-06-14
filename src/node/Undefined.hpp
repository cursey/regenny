#pragma once

#include "Base.hpp"

namespace node {
class Undefined : public Base {
public:
    Undefined(Process& process, size_t size);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;

protected:
    size_t m_size{};
};
} // namespace node