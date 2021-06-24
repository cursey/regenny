#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "node/Pointer.hpp"

#include "MemoryUi.hpp"

MemoryUi::MemoryUi(
    genny::Sdk& sdk, genny::Struct* struct_, Process& process, uintptr_t address, node::Property& inherited_props)
    : m_sdk{sdk}, m_struct{struct_}, m_process{process}, m_address{address}, m_props{inherited_props} {
    if (m_struct == nullptr) {
        return;
    }

    m_proxy_variable = std::make_unique<genny::Variable>("");
    m_proxy_variable->type(m_struct->ptr());

    auto root = std::make_unique<node::Pointer>(m_process, m_proxy_variable.get(), m_props);
    root->is_collapsed(false);

    m_root = std::move(root);
}

void MemoryUi::display() {
    if constexpr (sizeof(void*) == 8) {
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%-16s %-8s %s", "Address", "Offset", "Bytes");
    } else {
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%-8s %-8s %s", "Address", "Offset", "Bytes");
    }

    /* ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "% 8s", "Offset");*/

    if (m_root != nullptr) {
        ImGui::BeginChild("MemoryUiRoot", ImGui::GetContentRegionAvail());
        m_root->display(m_address, 0, (std::byte*)&m_address);
        ImGui::EndChild();
    }
}
