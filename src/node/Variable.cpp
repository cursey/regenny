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

Variable::Variable(Process& process, genny::Variable* var) : Base{process}, m_var{var}, m_size{var->size()} {
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

    static std::string s{};

    s.clear();

    auto end = std::min(m_var->size(), sizeof(uintptr_t));

    for (auto i = 0; i < end; ++i) {
        fmt::format_to(std::back_inserter(s), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (end < m_var->size()) {
        s += "...";
    }

    std::array<std::vector<std::string>*, 2> metadatas{&m_var->metadata(), &m_var->type()->metadata()};

    for (auto&& metadata : metadatas) {
        for (auto&& md : *metadata) {
            if (md == "u8") {
                display_as<uint8_t>(s, mem);
            } else if (md == "u16") {
                display_as<uint16_t>(s, mem);
            } else if (md == "u32") {
                display_as<uint32_t>(s, mem);
            } else if (md == "u64") {
                display_as<uint64_t>(s, mem);
            } else if (md == "i8") {
                display_as<int8_t>(s, mem);
            } else if (md == "i16") {
                display_as<int16_t>(s, mem);
            } else if (md == "i32") {
                display_as<int32_t>(s, mem);
            } else if (md == "i64") {
                display_as<int64_t>(s, mem);
            } else if (md == "f32") {
                display_as<float>(s, mem);
            } else if (md == "f64") {
                display_as<double>(s, mem);
            }
        }
    }

    if (auto enum_ = dynamic_cast<genny::Enum*>(m_var->type())) {
        switch (m_size) {
        case 1:
            display_enum<uint8_t>(s, mem, enum_);
            break;
        case 2:
            display_enum<uint16_t>(s, mem, enum_);
            break;
        case 4:
            display_enum<uint32_t>(s, mem, enum_);
            break;
        case 8:
            display_enum<uint64_t>(s, mem, enum_);
            break;
        }
    }

    ImGui::TextUnformatted(s.c_str());
}

size_t Variable::size() {
    return m_size;
}
} // namespace node
