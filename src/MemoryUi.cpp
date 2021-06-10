#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "node/Struct.hpp"

#include "MemoryUi.hpp"

using namespace std::literals;

MemoryUi::MemoryUi(genny::Sdk& sdk, genny::Struct* struct_, Process& process, uintptr_t address)
    : m_sdk{sdk}, m_struct{struct_}, m_process{process}, m_address{address} {
    if (m_struct == nullptr) {
        return;
    }

    m_proxy_variable = std::make_unique<genny::Variable>("");
    m_proxy_variable->type(m_struct);

    auto root = std::make_unique<node::Struct>(m_proxy_variable.get());
    root->collapse(false);

    m_root = std::move(root);
}

void MemoryUi::display() {
    refresh_memory();

    m_root->display(m_address, 0, &m_mem[0]);
}

void MemoryUi::refresh_memory() {
    if (auto now = std::chrono::steady_clock::now(); now >= m_mem_refresh_time) {
        m_mem_refresh_time = now + 500ms;

        // Make sure our memory buffer is large enough (since the first refresh it wont be).
        m_mem.resize(m_struct->size());
        m_process.read(m_address, m_mem.data(), m_mem.size());
    }
}
