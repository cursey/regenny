#include <fmt/format.h>
#include <imgui.h>

#include "Undefined.hpp"

namespace node {
Undefined::Undefined(Process& process, Property& props, size_t size)
    : Base{process, props}, m_size{size}, m_original_size{size} {
    m_props["__size"].set_default(0);

    // If our inherited size_override isn't 0 we apply the override now.
    if (size_override() != 0) {
        m_size = size_override();
    }
}

void Undefined::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    // Normal unsplit display.
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(m_bytes_str.c_str());
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, m_preview_str.c_str());
    ImGui::EndGroup();

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
}

size_t Undefined::size() {
    return m_size;
}

void Undefined::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::on_refresh(address, offset, mem);

    // Normal unsplit refresh.
    m_bytes_str.clear();
    m_preview_str.clear();

    for (auto i = 0; i < m_size; ++i) {
        fmt::format_to(std::back_inserter(m_bytes_str), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (m_size == sizeof(uintptr_t)) {
        auto addr = *(uintptr_t*)mem;

        // RTTI
        if (auto tn = m_process.get_typename(addr); tn) {
            fmt::format_to(std::back_inserter(m_preview_str), "obj:{:s} ", *tn);
        }

        for (auto&& mod : m_process.modules()) {
            if (mod.start <= addr && addr <= mod.end) {
                fmt::format_to(std::back_inserter(m_preview_str), "<{}>+0x{:X}", mod.name, addr - mod.start);
                // Bail here so we don't try previewing this pointer as something else.
                return;
            }
        }

        for (auto&& allocation : m_process.allocations()) {
            if (allocation.start <= addr && addr <= allocation.end) {
                fmt::format_to(std::back_inserter(m_preview_str), "heap:0x{:X}", addr);

                // Bail here so we don't try previewing this pointer as something else.
                return;
            }
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