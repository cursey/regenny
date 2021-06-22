#include <imgui.h>
#include <imgui_internal.h>

#include "Base.hpp"

namespace node {
int Base::indentation_level = -1;

Base::Base(Process& process) : m_process{process} {
}

void Base::display_address_offset(uintptr_t address, uintptr_t offset) {
    if constexpr (sizeof(void*) == 8) {
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%016llX", address);
    } else {
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%08X", address);
    }

    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%08X", offset);
    apply_indentation();
}

void Base::apply_indentation() {
    if (indentation_level > 0) {
        auto g = ImGui::GetCurrentContext();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2{g->Style.IndentSpacing * indentation_level, g->FontSize});
    }
}
} // namespace node
