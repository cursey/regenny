#include <fmt/format.h>
#include <imgui.h>

#include "UndefinedBitfield.hpp"

namespace node {
template <typename T> void display_bits(std::string& s, size_t num_bits, uintptr_t offset, std::byte* mem) {
    s += "0b";

    auto data = *(T*)mem;
    auto start = (int)offset;
    auto end = (int)(offset + num_bits);

    for (auto i = end - 1; i >= start; --i) {
        if (data >> i & 1) {
            s += "1";
        } else {
            s += "0";
        }
    }
}

UndefinedBitfield::UndefinedBitfield(
    Config& cfg, Process& process, Property& props, size_t size, size_t bit_size, size_t bit_offset)
    : Base{cfg, process, props}, m_size{size}, m_bit_size{bit_size}, m_bit_offset{bit_offset} {
}

void UndefinedBitfield::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%s", m_display_str.c_str());
}

void UndefinedBitfield::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::update(address, offset, mem);
    m_display_str.clear();

    switch (m_size) {
    case 1:
        display_bits<uint8_t>(m_display_str, m_bit_size, m_bit_offset, mem);
        break;
    case 2:
        display_bits<uint16_t>(m_display_str, m_bit_size, m_bit_offset, mem);
        break;
    case 4:
        display_bits<uint32_t>(m_display_str, m_bit_size, m_bit_offset, mem);
        break;
    case 8:
        display_bits<uint64_t>(m_display_str, m_bit_size, m_bit_offset, mem);
        break;
    default:
        assert(0);
    }
}

} // namespace node