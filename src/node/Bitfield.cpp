#include <array>
#include <cassert>

#include <fmt/format.h>
#include <imgui.h>

#include "Bitfield.hpp"

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

template <typename T> void display_as(std::string& s, size_t num_bits, uintptr_t offset, std::byte* mem) {
    T mask{};
    auto data = *(T*)mem;
    auto start = offset;
    auto end = offset + num_bits;

    for (auto i = start; i < end; ++i) {
        mask |= 1ull << i;
    }

    data &= mask;
    data >>= start;

    fmt::format_to(std::back_inserter(s), " {}", data);
}

Bitfield::Bitfield(Process& process, genny::Variable* var, Property& props) : Variable{process, var, props} {
    assert(var->is_bitfield());
}

void Bitfield::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, m_var->type()->name().c_str());
    ImGui::SameLine();
    ImGui::Text("%s : %d", m_var->name().c_str(), m_var->bit_size());
    ImGui::SameLine();
    ImGui::TextUnformatted(m_display_str.c_str());
}

void Bitfield::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    m_display_str.clear();

    switch (m_var->type()->size()) {
    case 1:
        display_bits<uint8_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
        break;
    case 2:
        display_bits<uint16_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
        break;
    case 4:
        display_bits<uint32_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
        break;
    case 8:
        display_bits<uint64_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
        break;
    default:
        assert(0);
    }

    std::array<std::vector<std::string>*, 2> metadatas{&m_var->metadata(), &m_var->type()->metadata()};

    for (auto&& metadata : metadatas) {
        for (auto&& md : *metadata) {
            if (md == "u8") {
                display_as<uint8_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "u16") {
                display_as<uint16_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "u32") {
                display_as<uint32_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "u64") {
                display_as<uint64_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "i8") {
                display_as<int8_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "i16") {
                display_as<int16_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "i32") {
                display_as<int32_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            } else if (md == "i64") {
                display_as<int64_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem);
            }
        }
    }
}
} // namespace node