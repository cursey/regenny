#include <imgui.h>
#include <imgui_internal.h>

#include "Base.hpp"

namespace node {
int Base::indentation_level = 0;

void Base::display_address_offset(uintptr_t address, uintptr_t offset) {
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%016llX", address);
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%8X", offset);

    if (indentation_level > 0) {
        auto g = ImGui::GetCurrentContext();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2{g->Style.IndentSpacing * indentation_level, g->FontSize});
    }
}
} // namespace node
