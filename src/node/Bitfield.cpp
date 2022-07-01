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

template <typename T>
void display_enum(std::string& s, size_t num_bits, uintptr_t offset, std::byte* mem, genny::Enum* enum_) {
    T mask{};
    auto data = *(T*)mem;
    auto start = offset;
    auto end = offset + num_bits;

    for (auto i = start; i < end; ++i) {
        mask |= 1ull << i;
    }

    data &= mask;
    data >>= start;

    auto val_found = false;

    for (auto&& [val_name, val_val] : enum_->values()) {
        if (val_val == data) {
            s += ' ' + val_name;
            val_found = true;
            break;
        }
    }

    if (!val_found) {
        display_as<T>(s, num_bits, offset, mem);
    }
}

Bitfield::Bitfield(Config& cfg, Process& process, genny::Variable* var, Property& props)
    : Variable{cfg, process, var, props} {
    assert(var->is_bitfield());
}

void Bitfield::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, "%s", m_var->type()->name().c_str());
    ImGui::SameLine();
    ImGui::Text("%s : %d", m_var->name().c_str(), m_var->bit_size());
    ImGui::SameLine();
    ImGui::TextUnformatted(m_display_str.c_str());
    ImGui::EndGroup();

    if (ImGui::BeginPopupContextItem("BitfieldNodes")) {
        write_display(address, mem);
        ImGui::EndPopup();
    }
}

void Bitfield::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::update(address, offset, mem);
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

    if (auto enum_ = dynamic_cast<genny::Enum*>(m_var->type())) {
        switch (m_var->type()->size()) {
        case 1:
            display_enum<uint8_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem, enum_);
            break;
        case 2:
            display_enum<uint16_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem, enum_);
            break;
        case 4:
            display_enum<uint32_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem, enum_);
            break;
        case 8:
            display_enum<uint64_t>(m_display_str, m_var->bit_size(), m_var->bit_offset(), mem, enum_);
            break;
        }
    }
}

template <typename T>
void handle_write(Process& process, size_t num_bits, uintptr_t offset, uintptr_t address, std::byte* mem) {
    T mask{};
    auto data = *(T*)mem;
    auto start = offset;
    auto end = offset + num_bits;

    for (auto i = start; i < end; ++i) {
        mask |= 1ull << i;
    }

    data &= mask;
    data >>= start;
    auto value = *(T*)mem;
    ImGuiDataType datatype;

    if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, bool>) {
        datatype = ImGuiDataType_U8;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        datatype = ImGuiDataType_U16;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        datatype = ImGuiDataType_U32;
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        datatype = ImGuiDataType_U64;
    } else if constexpr (std::is_same_v<T, int8_t>) {
        datatype = ImGuiDataType_S8;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        datatype = ImGuiDataType_S16;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        datatype = ImGuiDataType_S32;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        datatype = ImGuiDataType_S64;
    }

    if (ImGui::InputScalar(
            "Value", datatype, (void*)&data, nullptr, nullptr, nullptr, ImGuiInputTextFlags_EnterReturnsTrue)) {
        value &= ~mask;
        value |= (data << start) & mask;
        process.write(address, (const void*)&value, sizeof(T));

        // Write it back to the mem so the next frame it displays the new value (if user hit enter).
        *(T*)mem = value;
    }
}

void Bitfield::write_display(uintptr_t address, std::byte* mem) {
    switch (m_var->type()->size()) {
    case 1:
        handle_write<uint8_t>(m_process, m_var->bit_size(), m_var->bit_offset(), address, mem);
        break;
    case 2:
        handle_write<uint16_t>(m_process, m_var->bit_size(), m_var->bit_offset(), address, mem);
        break;
    case 4:
        handle_write<uint32_t>(m_process, m_var->bit_size(), m_var->bit_offset(), address, mem);
        break;
    case 8:
        handle_write<uint64_t>(m_process, m_var->bit_size(), m_var->bit_offset(), address, mem);
        break;
    default:
        assert(0);
    }
}
} // namespace node