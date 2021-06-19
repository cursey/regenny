#include <array>
#include <cassert>

#include <fmt/format.h>
#include <imgui.h>

#include "Bitfield.hpp"

namespace node {
template <typename T> void display_bits(std::string& s, size_t num_bits, uintptr_t offset, std::byte* mem) {
    s += "0b";

    static std::string bit_str{};

    bit_str.clear();

    auto data = *(T*)mem;
    auto start = offset;
    auto end = offset + num_bits;

    // This check is for detecting if the bitfield stradles a boundry. Bitfields that stradle a boundry are aligned to
    // that boundry. I'm not sure if I want to handle this here or just have users maintain bitfield boundries
    // themselves.
    /*if (end > sizeof(T) * CHAR_BIT) {
        data = *((T*)mem + 1);
        start = 0;
        end = num_bits;
    }*/

    for (auto i = start; i < end; ++i) {
        if (data >> i & 1) {
            bit_str += "1";
        } else {
            bit_str += "0";
        }
    }

    std::reverse(bit_str.begin(), bit_str.end());

    s += bit_str;
}

template <typename T> void display_as(std::string& s, size_t num_bits, uintptr_t offset, std::byte* mem) {
    T head{};
    T tail{};
    T mask{};
    auto data = *(T*)mem;
    auto start = offset;
    auto end = offset + num_bits;

    // This check is for detecting if the bitfield stradles a boundry. Bitfields that stradle a boundry are aligned to
    // that boundry. I'm not sure if I want to handle this here or just have users maintain bitfield boundries
    // themselves.
    /*if (end > sizeof(T) * CHAR_BIT) {
        data = *((T*)mem + 1);
        start = 0;
        end = num_bits;
    }*/

    for (auto i = start; i < end; ++i) {
        mask |= 1ull << i;
    }

    data &= mask;
    data >>= start;
    head = data;

    fmt::format_to(std::back_inserter(s), " {}", data);
}

Bitfield::Bitfield(Process& process, genny::Bitfield* bf) : Variable{process, bf}, m_bf{bf} {
    for (auto&& field : bf->get_all<genny::Bitfield::Field>()) {
        m_fields[field->offset()] = std::make_unique<Field>(m_process, field);
    }
}

void Bitfield::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    auto type_size = m_bf->type()->size() * CHAR_BIT;

    for (auto&& [_, field] : m_fields) {
        auto genny_field = field->field();
        auto field_offset = genny_field->offset();
        auto mem_offset = field_offset / type_size * type_size;

        field->display(address + mem_offset, offset + mem_offset, mem + mem_offset);
    }
}

void Bitfield::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    auto type_size = m_bf->type()->size() * CHAR_BIT;

    for (auto&& [_, field] : m_fields) {
        auto genny_field = field->field();
        auto field_offset = genny_field->offset();
        auto mem_offset = field_offset / type_size * type_size;

        field->on_refresh(address + mem_offset, offset + mem_offset, mem + mem_offset);
    }
}

Bitfield::Field::Field(Process& process, genny::Bitfield::Field* field) : Base{process}, m_field{field} {
}

void Bitfield::Field::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, m_field->owner<genny::Bitfield>()->type()->name().c_str());
    ImGui::SameLine();
    ImGui::Text("%s : %d", m_field->name().c_str(), m_field->size());
    ImGui::SameLine();
    ImGui::TextUnformatted(m_display_str.c_str());
}

size_t Bitfield::Field::size() {
    return m_field->size();
}

void Bitfield::Field::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    m_display_str.clear();

    switch (m_field->owner<genny::Bitfield>()->type()->size()) {
    case 1:
        display_bits<uint8_t>(m_display_str, size(), m_field->offset(), mem);
        break;
    case 2:
        display_bits<uint16_t>(m_display_str, size(), m_field->offset(), mem);
        break;
    case 4:
        display_bits<uint32_t>(m_display_str, size(), m_field->offset(), mem);
        break;
    case 8:
        display_bits<uint64_t>(m_display_str, size(), m_field->offset(), mem);
        break;
    default:
        assert(0);
    }

    auto bf = m_field->owner<genny::Bitfield>();
    std::array<std::vector<std::string>*, 2> metadatas{&bf->metadata(), &bf->type()->metadata()};

    for (auto&& metadata : metadatas) {
        for (auto&& md : *metadata) {
            if (md == "u8") {
                display_as<uint8_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "u16") {
                display_as<uint16_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "u32") {
                display_as<uint32_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "u64") {
                display_as<uint64_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "i8") {
                display_as<int8_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "i16") {
                display_as<int16_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "i32") {
                display_as<int32_t>(m_display_str, size(), m_field->offset(), mem);
            } else if (md == "i64") {
                display_as<int64_t>(m_display_str, size(), m_field->offset(), mem);
            }
        }
    }
}
} // namespace node