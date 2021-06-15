#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "node/Pointer.hpp"

#include "MemoryUi.hpp"

MemoryUi::MemoryUi(genny::Sdk& sdk, genny::Struct* struct_, Process& process, uintptr_t address)
    : m_sdk{sdk}, m_struct{struct_}, m_process{process}, m_address{address} {
    if (m_struct == nullptr) {
        return;
    }

    m_proxy_variable = std::make_unique<genny::Variable>("");
    m_proxy_variable->type(m_struct->ptr());

    auto root = std::make_unique<node::Pointer>(m_process, m_proxy_variable.get());
    root->collapse(false);

    m_root = std::move(root);
}

void MemoryUi::display() {
    if (m_root != nullptr) {
        m_root->display(m_address, 0, (std::byte*)&m_address);
    }
}
