#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <utf8.h>

#include "Variable.hpp"

namespace node {
template <typename T> void display_as(std::string& s, std::byte* mem) {
    fmt::format_to(std::back_inserter(s), "{} ", *(T*)mem);
}

static void display_str(std::string& s, const std::string& str) {
    s += "\"";

    for (auto&& c : str) {
        if (c == 0) {
            break;
        }

        s += c;
    }

    s += "\" ";
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

Variable::Variable(Config& cfg, Process& process, genny::Variable* var, Property& props)
    : Base{cfg, process, props}, m_var{var}, m_size{var->size()} {
}

void Variable::display_type() {
    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, "%s", m_var->type()->name().c_str());
}

void Variable::display_name() {
    ImGui::TextUnformatted(m_var->name().c_str());
}

void Variable::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::BeginGroup();
    display_type();
    ImGui::SameLine();
    display_name();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, {181.0f / 255.0f, 206.0f / 255.0f, 168.0f / 255.0f, 1.0f});
    ImGui::TextUnformatted(m_value_str.c_str());
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    if (ImGui::BeginPopupContextItem("VariableNodes")) {
        write_display(address, mem);
        ImGui::EndPopup();
    }
}

size_t Variable::size() {
    return m_size;
}

void Variable::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::update(address, offset, mem);

    m_value_str.clear();

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

                m_utf16.back() = L'\0';

                // if we don't do this then utf16to8 will throw an exception.
                // todo: do for utf32?
                const auto real_len = wcslen((wchar_t*)m_utf16.data());
                m_utf16.resize(real_len);

                std::string utf8conv{};

                try {
                    utf8conv = utf8::utf16to8(m_utf16);
                } catch (utf8::invalid_utf16& e) {
                    utf8conv = e.what();
                }

                display_str(m_value_str, utf8conv);
            } else if (md == "utf32*") {
                m_utf32.resize(256);
                m_process.read(*(uintptr_t*)mem, m_utf32.data(), 255 * sizeof(char32_t));

                std::string utf32conv{};

                try {
                    utf32conv = utf8::utf32to8(m_utf32);
                } catch (utf8::invalid_utf16& e) {
                    utf32conv = e.what();
                }

                display_str(m_value_str, utf32conv);
            } else if (md == "bool") {
                if (*(bool*)mem) {
                    m_value_str += "true ";
                } else {
                    m_value_str += "false ";
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

template <typename T> void handle_write(Process& process, uintptr_t address, std::byte* mem) {
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
    } else if constexpr (std::is_same_v<T, float>) {
        datatype = ImGuiDataType_Float;
    } else if constexpr (std::is_same_v<T, double>) {
        datatype = ImGuiDataType_Double;
    }

    if (ImGui::InputScalar(
            "Value", datatype, (void*)&value, nullptr, nullptr, nullptr, ImGuiInputTextFlags_EnterReturnsTrue)) {
        process.write(address, (const void*)&value, sizeof(T));

        // Write it back to the mem so the next frame it displays the new value (if user hit enter).
        *(T*)mem = value;
    }
}

void Variable::write_display(uintptr_t address, std::byte* mem) {
    std::array<std::vector<std::string>*, 2> metadatas{&m_var->metadata(), &m_var->type()->metadata()};

    for (auto&& metadata : metadatas) {
        for (auto&& md : *metadata) {
            if (md == "u8") {
                handle_write<uint8_t>(m_process, address, mem);
            } else if (md == "u16") {
                handle_write<uint16_t>(m_process, address, mem);
            } else if (md == "u32") {
                handle_write<uint32_t>(m_process, address, mem);
            } else if (md == "u64") {
                handle_write<uint64_t>(m_process, address, mem);
            } else if (md == "i8") {
                handle_write<int8_t>(m_process, address, mem);
            } else if (md == "i16") {
                handle_write<int16_t>(m_process, address, mem);
            } else if (md == "i32") {
                handle_write<int32_t>(m_process, address, mem);
            } else if (md == "i64") {
                handle_write<int64_t>(m_process, address, mem);
            } else if (md == "f32") {
                handle_write<float>(m_process, address, mem);
            } else if (md == "f64") {
                handle_write<double>(m_process, address, mem);
            } else if (md == "bool") {
                handle_write<bool>(m_process, address, mem);
            } else {
                ImGui::Text("Unable to write to this data type");
            }
        }
    }
}
} // namespace node
