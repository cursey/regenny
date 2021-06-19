#include <array>

#include <fmt/format.h>
#include <imgui.h>

#include "Variable.hpp"

namespace node {
template <typename T> void display_as(std::string& s, std::byte* mem) {
    fmt::format_to(std::back_inserter(s), " {}", *(T*)mem);
}

template <typename T> void display_enum(std::string& s, std::byte* mem, genny::Enum* enum_) {
    auto val = *(T*)mem;
    auto val_found = false;

    for (auto&& [val_name, val_val] : enum_->values()) {
        if (val_val == val) {
            s += ' ' + val_name;
            val_found = true;
            break;
        }
    }

    if (!val_found) {
        display_as<T>(s, mem);
    }
}

Variable::Variable(Process& process, genny::Variable* var, Property& props)
    : Base{process}, m_var{var}, m_size{var->size()}, m_props{props} {
}

void Variable::display_type() {
    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, m_var->type()->name().c_str());
}

void Variable::display_name() {
    ImGui::TextUnformatted(m_var->name().c_str());
}

void Variable::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    display_type();
    ImGui::SameLine();
    display_name();
    ImGui::SameLine();
    ImGui::TextUnformatted(m_value_str.c_str());
}

size_t Variable::size() {
    return m_size;
}

void Variable::on_refresh(uintptr_t addresm_value_str, uintptr_t offset, std::byte* mem) {
    m_value_str.clear();

    auto end = std::min(m_var->size(), sizeof(uintptr_t));

    for (auto i = 0; i < end; ++i) {
        fmt::format_to(std::back_inserter(m_value_str), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (end < m_var->size()) {
        m_value_str += "...";
    }

    std::array<std::vector<std::string>*, 2> metadatas{&m_var->metadata(), &m_var->type()->metadata()};

    for (auto&& metadata : metadatas) {
        for (auto&& md : *metadata) {
            if (md == "u8") {
                display_as<uint8_t>(m_value_str, mem);
            } else if (md == "u16") {
                display_as<uint16_t>(m_value_str, mem);
            } else if (md == "u32") {
                display_as<uint32_t>(m_value_str, mem);
            } else if (md == "u64") {
                display_as<uint64_t>(m_value_str, mem);
            } else if (md == "i8") {
                display_as<int8_t>(m_value_str, mem);
            } else if (md == "i16") {
                display_as<int16_t>(m_value_str, mem);
            } else if (md == "i32") {
                display_as<int32_t>(m_value_str, mem);
            } else if (md == "i64") {
                display_as<int64_t>(m_value_str, mem);
            } else if (md == "f32") {
                display_as<float>(m_value_str, mem);
            } else if (md == "f64") {
                display_as<double>(m_value_str, mem);
            }
        }
    }

    if (auto enum_ = dynamic_cast<genny::Enum*>(m_var->type())) {
        switch (m_size) {
        case 1:
            display_enum<uint8_t>(m_value_str, mem, enum_);
            break;
        case 2:
            display_enum<uint16_t>(m_value_str, mem, enum_);
            break;
        case 4:
            display_enum<uint32_t>(m_value_str, mem, enum_);
            break;
        case 8:
            display_enum<uint64_t>(m_value_str, mem, enum_);
            break;
        }
    }
}
} // namespace node
