#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "node/Pointer.hpp"

#include "MemoryUi.hpp"

MemoryUi::MemoryUi(
    genny::Sdk& sdk, genny::Struct* struct_, Process& process, node::Property& inherited_props)
    : m_sdk{sdk}, m_struct{struct_}, m_process{process}, m_props{inherited_props} {
    if (m_struct == nullptr) {
        return;
    }

    m_proxy_variable = std::make_unique<genny::Variable>("root");
    m_proxy_variable->type(m_struct->ptr());

    auto root = std::make_unique<node::Pointer>(m_process, m_proxy_variable.get(), m_props);
    root->is_collapsed(false);

    m_root = std::move(root);
}

void MemoryUi::display(uintptr_t address) {
    if constexpr (sizeof(void*) == 8) {
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%-16s %-8s %s", "Address", "Offset", "Bytes");
    } else {
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%-8s %-8s %s", "Address", "Offset", "Bytes");
    }

    if (m_root != nullptr) {
        ImGui::BeginChild("MemoryUiRoot", ImGui::GetContentRegionAvail());
        m_root->display(address, 0, (std::byte*)&address);
        ImGui::EndChild();
    }
}
