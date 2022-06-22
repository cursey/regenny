#include <fmt/format.h>
#include <imgui.h>

#include "Pointer.hpp"

#include "Undefined.hpp"

namespace node {
static genny::Namespace g_preview_ns{""};
static genny::Variable g_preview_ptr{"preview_ptr"};
static Property g_preview_props{};
static std::unique_ptr<Pointer> g_preview_node{};
static Process* g_preview_node_process{};
bool Undefined::is_hidden{};

Undefined::Undefined(Config& cfg, Process& process, Property& props, size_t size)
    : Base{cfg, process, props}, m_size{size}, m_original_size{size} {
    m_props["__size"].set_default(0);

    // If our inherited size_override isn't 0 we apply the override now.
    if (size_override() != 0) {
        m_size = size_override();
    }

    if (g_preview_node == nullptr || g_preview_node_process != &process) {
        auto preview_struct = g_preview_ns.struct_("preview")->size(sizeof(uintptr_t) * 16);
        g_preview_ptr.type(preview_struct->ptr());
        g_preview_node = std::make_unique<Pointer>(m_cfg, process, &g_preview_ptr, g_preview_props);
        g_preview_node->is_collapsed() = false;
        g_preview_node_process = &process;
    }
}

void Undefined::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (is_hidden) {
        return;
    }

    // Normal unsplit display.
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(m_bytes_str.c_str());
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%s", m_preview_str.c_str());
    ImGui::EndGroup();

    auto is_hovered = ImGui::IsItemHovered();

    if (ImGui::BeginPopupContextItem("UndefinedNodes")) {
        if (ImGui::InputInt("Size Override", &size_override())) {
            size_override() = std::clamp(size_override(), 0, 8);

            if (size_override() == 0) {
                m_size = m_original_size;
            } else {
                m_size = size_override();
            }
        }

        ImGui::EndPopup();
    }

    if (is_hovered && m_is_pointer) {
        auto backup_indentation_level = indentation_level;

        ImGui::BeginTooltip();
        indentation_level = -1;
        g_preview_node->display(*(uintptr_t*)mem, 0, &mem[0]);
        ImGui::EndTooltip();

        indentation_level = backup_indentation_level;
    }
}

size_t Undefined::size() {
    return m_size;
}

void Undefined::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::update(address, offset, mem);

    m_is_pointer = false;

    // Normal unsplit refresh.
    m_bytes_str.clear();
    m_preview_str.clear();

    for (auto i = 0; i < m_size; ++i) {
        fmt::format_to(std::back_inserter(m_bytes_str), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (m_size == sizeof(uintptr_t)) {
        auto addr = *(uintptr_t*)mem;

        // RTTI
        if (auto tn = m_process.get_typename(address); tn) {
            fmt::format_to(std::back_inserter(m_preview_str), "obj:{:s} ", *tn);
        }

        if (auto tn = m_process.get_typename(addr); tn) {
            fmt::format_to(std::back_inserter(m_preview_str), "obj*:{:s} ", *tn);
        }

        for (auto&& mod : m_process.modules()) {
            if (mod.start <= addr && addr <= mod.end) {
                fmt::format_to(std::back_inserter(m_preview_str), "<{}>+0x{:X} ", mod.name, addr - mod.start);
                m_is_pointer = true;
            }
        }

        for (auto&& allocation : m_process.allocations()) {
            if (allocation.start <= addr && addr <= allocation.end) {
                fmt::format_to(std::back_inserter(m_preview_str), "heap:0x{:X} ", addr);
                m_is_pointer = true;
            }
        }

        if (m_is_pointer) {
            // See if it looks like its pointing to a string.
            static std::string str{};
            str.resize(256);
            m_process.read(addr, str.data(), 255 * sizeof(char));
            str.resize(std::min<size_t>(256, strlen(str.data())));

            auto is_str = true;

            for (auto&& c : str) {
                if (!isprint((uint8_t)c)) {
                    is_str = false;
                    break;
                }
            }

            if (is_str) {
                fmt::format_to(std::back_inserter(m_preview_str), "\"{}\" ", str);
            }

            // Bail here so we don't try previewing this pointer as something else.
            return;
        }
    }

    switch (m_size) {
    case 8:
        fmt::format_to(std::back_inserter(m_preview_str), "i64:{} f64:{}", *(int64_t*)mem, *(double*)mem);
        break;
    case 4:
        fmt::format_to(std::back_inserter(m_preview_str), "i32:{} f32:{}", *(int32_t*)mem, *(float*)mem);
        break;
    case 2:
        fmt::format_to(std::back_inserter(m_preview_str), "i16:{}", *(int16_t*)mem);
        break;
    case 1: {
        auto val = *(int8_t*)mem;
        fmt::format_to(std::back_inserter(m_preview_str), "i8:{}", val);

        if (0x20 <= val && val <= 0x7E) {
            fmt::format_to(std::back_inserter(m_preview_str), " '{}'", (char)val);
        }
    } break;
    }
}
} // namespace node