#include <SDL.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
// msvc 15 errors about min/max
#include <algorithm>

#include "Base.hpp"

namespace node {
int Base::indentation_level = -1;

Base::Base(Config& cfg, Process& process, Property& props) : m_cfg{cfg}, m_process{process}, m_props{props} {
}

void Base::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    m_preamble_str.clear();
    m_bytes_str.clear();
    m_print_str.clear();

    auto end = (int)std::min(size(), sizeof(uint64_t));

    for (auto i = end - 1; i >= 0; --i) {
        fmt::format_to(std::back_inserter(m_bytes_str), "{:02X}", *(uint8_t*)&mem[i]);
    }

    if (end < size()) {
        m_bytes_str += "...";
    }

    for (auto i = 0; i < end; ++i) {
        auto c = *(char*)(mem + i);

        if (c >= 0 && isprint(c)) {
            m_print_str += c;
        } else {
            m_print_str += '.';
        }
    }

    auto needs_space = false;

    if (m_cfg.display_address) {
        if constexpr (sizeof(void*) == 8) {
            fmt::format_to(std::back_inserter(m_preamble_str), "{:016X}", address);
        } else {
            fmt::format_to(std::back_inserter(m_preamble_str), "{:08X}", address);
        }

        needs_space = true;
    }

    if (m_cfg.display_offset) {
        if (needs_space) {
            m_preamble_str += ' ';
        }

        fmt::format_to(std::back_inserter(m_preamble_str), "{:08X}", offset);
        needs_space = true;
    }

    if (m_cfg.display_bytes) {
        if (needs_space) {
            m_preamble_str += ' ';
        }

        fmt::format_to(std::back_inserter(m_preamble_str), "{:19}", m_bytes_str.c_str());
        needs_space = true;
    }

    if (m_cfg.display_print) {
        if (needs_space) {
            m_preamble_str += ' ';
        }

        fmt::format_to(std::back_inserter(m_preamble_str), "{:8}", m_print_str.c_str());
        needs_space = true;
    }
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

    if (indentation_level > 0) {
        auto g = ImGui::GetCurrentContext();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2{g->Style.IndentSpacing * indentation_level, g->FontSize});
    }
}

} // namespace node
