#pragma once

#include "Base.hpp"

namespace node {
class UndefinedBitfield : public Base {
public:
    UndefinedBitfield(Config& cfg, Process& process, Property& props, size_t size, size_t bit_size, size_t bit_offset);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override { return m_size; }
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

protected:
    size_t m_size{};
    size_t m_bit_size{};
    size_t m_bit_offset{};
    std::string m_display_str{};
};
} // namespace node