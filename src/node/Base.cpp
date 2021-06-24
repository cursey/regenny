#include <SDL.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "Base.hpp"

namespace node {
int Base::indentation_level = -1;

Base::Base(Process& process, Property& props) : m_process{process}, m_props{props} {
}

void Base::display_address_offset(uintptr_t address, uintptr_t offset) {
    ImGui::PushStyleColor(ImGuiCol_Text, {0.6f, 0.6f, 0.6f, 1.0f});
    ImGui::TextUnformatted(m_preamble_str.c_str());
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem("Preamble")) {
        if (ImGui::Button("Copy Address")) {
            SDL_SetClipboardText(fmt::format("0x{:X}", address).c_str());
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Copy Offset")) {
            SDL_SetClipboardText(fmt::format("0x{:X}", offset).c_str());
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Copy Bytes")) {
            SDL_SetClipboardText(m_bytes_str.c_str());
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    apply_indentation();
}

void Base::apply_indentation() {
    if (indentation_level > 0) {
        auto g = ImGui::GetCurrentContext();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2{g->Style.IndentSpacing * indentation_level, g->FontSize});
    }
}

void Base::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    m_preamble_str.clear();
    m_bytes_str.clear();

    if constexpr (sizeof(void*) == 8) {
        fmt::format_to(std::back_inserter(m_preamble_str), "{:016X} {:08X}", address, offset);
    } else {
        fmt::format_to(std::back_inserter(m_preamble_str), "{:08X} {:08X}", address, offset);
    }

    auto end = (int)std::min(size(), sizeof(uint64_t));

    for (auto i = end - 1; i >= 0; --i) {
        fmt::format_to(std::back_inserter(m_bytes_str), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (end < size()) {
        m_bytes_str += "...";
    }

    fmt::format_to(std::back_inserter(m_preamble_str), " {:19}", m_bytes_str.c_str());
}
} // namespace node
