#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "node/Pointer.hpp"

#include "MemoryUi.hpp"

MemoryUi::MemoryUi(
    Config& cfg, genny::Sdk& sdk, genny::Struct* struct_, Process& process, node::Property& inherited_props)
    : m_cfg{cfg}, m_sdk{sdk}, m_struct{struct_}, m_process{process}, m_props{inherited_props} {
    if (m_struct == nullptr) {
        return;
    }

    m_proxy_variable = std::make_unique<genny::Variable>("root");
    m_proxy_variable->type(m_struct->ptr());

    auto root = std::make_unique<node::Pointer>(m_cfg, m_process, m_proxy_variable.get(), m_props);
    root->is_collapsed(false);

    m_root = std::move(root);
}

void MemoryUi::display(uintptr_t address) {
    m_header.clear();

    auto needs_space = false;

    if (m_cfg.display_address) {
        if constexpr (sizeof(void*) == 8) {
            fmt::format_to(std::back_inserter(m_header), "{:16}", "Address");
        } else {
            fmt::format_to(std::back_inserter(m_header), "{:8}", "Address");
        }

        needs_space = true;
    }

    if (m_cfg.display_offset) {
        if (needs_space) {
            m_header += ' ';
        }

        fmt::format_to(std::back_inserter(m_header), "{:8}", "Offset");
        needs_space = true;
    }

    if (m_cfg.display_bytes) {
        if (needs_space) {
            m_header += ' ';
        }

        fmt::format_to(std::back_inserter(m_header), "{:19}", "Bytes");
        needs_space = true;
    }

    if (m_cfg.display_print) {
        if (needs_space) {
            m_header += ' ';
        }

        fmt::format_to(std::back_inserter(m_header), "{:8}", "Print");
        needs_space = true;
    }

    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%s", m_header.c_str());

    if (m_root != nullptr) {
        ImGui::BeginChild("MemoryUiRoot", ImGui::GetContentRegionAvail());
        m_root->display(address, 0, (std::byte*)&address);
        ImGui::EndChild();
    }
}
