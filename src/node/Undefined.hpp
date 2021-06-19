#pragma once

#include "Base.hpp"

namespace node {
class Undefined : public Base {
public:
    Undefined(Process& process, size_t size);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

protected:
    size_t m_size{};
    std::string m_display_str{};
};
} // namespace node