#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <utf8.h>

#include "Variable.hpp"

namespace node {
template <typename T> void display_as(std::string& s, std::byte* mem) {
    fmt::format_to(std::back_inserter(s), " {}", *(T*)mem);
}

void display_str(std::string& s, const std::string& str) {
    s += " \"";

    for (auto&& c : str) {
        if (c == 0) {
            break;
        }

        s += c;
    }

    s += "\"";
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

void Variable::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    m_value_str.clear();

    auto end = (int)std::min(m_var->size(), sizeof(uintptr_t));

    for (auto i = end - 1; i >= 0; --i) {
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
            } else if (md == "utf8*") {
                m_utf8.resize(256);
                m_process.read(*(uintptr_t*)mem, m_utf8.data(), 255 * sizeof(char));
                display_str(m_value_str, m_utf8);
            } else if (md == "utf16*") {
                m_utf16.resize(256);
                m_process.read(*(uintptr_t*)mem, m_utf16.data(), 255 * sizeof(char16_t));
                display_str(m_value_str, utf8::utf16to8(m_utf16));
            } else if (md == "utf32*") {
                m_utf32.resize(256);
                m_process.read(*(uintptr_t*)mem, m_utf32.data(), 255 * sizeof(char32_t));
                display_str(m_value_str, utf8::utf32to8(m_utf32));
            } else if (md == "bool") {
                if (*(bool*)mem) {
                    m_value_str += " true";
                } else {
                    m_value_str += " false";
                }
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
