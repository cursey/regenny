#include <fmt/format.h>
#include <imgui.h>

#include "Undefined.hpp"

namespace node {
Undefined::Undefined(Process& process, Property& props, size_t size, Undefined* parent)
    : Base{process, props}, m_size{size}, m_parent{parent} {
    if (m_props["__split"].value.index() == 0) {
        is_split(false);
    }
}

void Undefined::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    // Reset if this node has become unsplit.
    if (!is_split() && m_split0 != nullptr) {
        m_split0.reset();
        m_split1.reset();
        m_props["split0"] = {};
        m_props["split1"] = {};
    }

    // Split display.
    if (m_split0 != nullptr) {
        auto size = m_size / 2;

        ImGui::PushID(m_split0.get());
        m_split0->display(address, offset, mem);
        ImGui::PopID();
        ImGui::PushID(m_split1.get());
        m_split1->display(address + size, offset + size, mem + size);
        ImGui::PopID();
        return;
    }

    // Normal unsplit display.
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(m_bytes_str.c_str());
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, m_preview_str.c_str());
    ImGui::EndGroup();

    if (ImGui::BeginPopupContextItem("UndefinedNodes")) {
        if (m_size > 1) {
            if (ImGui::Button("Split")) {
                is_split() = true;
                m_split0 = std::make_unique<Undefined>(m_process, m_props["split0"], m_size / 2, this);
                m_split1 = std::make_unique<Undefined>(m_process, m_props["split1"], m_size / 2, this);
            }
        }

        if (m_parent != nullptr) {
            if (ImGui::Button("Unsplit")) {
                m_parent->is_split() = false;
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

    // Split refresh.
    if (m_split0 != nullptr) {
        auto size = m_size / 2;
        m_split0->on_refresh(address, offset, mem);
        m_split1->on_refresh(address + size, offset + size, mem + size);
        return;
    }

    // Normal unsplit refresh.
    m_bytes_str.clear();
    m_preview_str.clear();

    for (auto i = 0; i < m_size; ++i) {
        fmt::format_to(std::back_inserter(m_bytes_str), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (m_size == sizeof(uintptr_t)) {
        auto addr = *(uintptr_t*)mem;

        // RTTI
        if (auto tn = m_process.get_typename((void*)addr); tn) {
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