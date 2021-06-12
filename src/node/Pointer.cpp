#include <imgui.h>

#include "Array.hpp"
#include "Struct.hpp"

#include "Pointer.hpp"

using namespace std::literals;

namespace node {
Pointer::Pointer(Process& process, genny::Variable* var) : Variable{process, var} {
    m_ptr = dynamic_cast<genny::Pointer*>(m_var->type());
    assert(m_ptr != nullptr);

    m_proxy_var = std::make_unique<genny::Variable>(m_var->name());
    m_proxy_var->type(m_ptr->to());

    std::unique_ptr<Variable> node{};

    if (m_ptr->to()->is_a<genny::Struct>()) {
        auto struct_ = std::make_unique<Struct>(m_process, m_proxy_var.get());
        struct_->display_self(false);
        m_ptr_node = std::move(struct_);
    } else if (m_ptr->to()->is_a<genny::Pointer>()) {
        m_ptr_node = std::make_unique<Pointer>(m_process, m_proxy_var.get());
    } else {
        m_ptr_node = std::make_unique<Variable>(m_process, m_proxy_var.get());
    }
}

void Pointer::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (indentation_level >= 0) {
        ImGui::BeginGroup();
        display_address_offset(address, offset);
        ImGui::SameLine();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::SameLine();
        ImGui::Text("%p", *(uintptr_t*)mem);

        ImGui::EndGroup();

        if (ImGui::BeginPopupContextItem("ArrayNode")) {
            ImGui::Checkbox("Collpase", &m_is_collapsed);
            ImGui::EndPopup();
        }

        if (m_is_collapsed) {
            return;
        }
    }

    auto pointed_to_address = *(uintptr_t*)mem;

    if (pointed_to_address != m_address) {
        m_address = pointed_to_address;
    }

    refresh_memory();

    // This can happen if the type pointed to is empty. For example if the user has just created the type in the editor
    // and the memory ui has been refreshed.
    if (m_mem.empty()) {
        return;
    }

    ++indentation_level;
    ImGui::PushID(m_ptr_node.get());
    // node->display(address + offset, offset, &mem[offset]);
    m_ptr_node->display(m_address, 0, &m_mem[0]);
    ImGui::PopID();
    --indentation_level;
}

void Pointer::refresh_memory() {
    if (auto now = std::chrono::steady_clock::now(); now >= m_mem_refresh_time) {
        m_mem_refresh_time = now + 500ms;

        // Make sure our memory buffer is large enough (since the first refresh it wont be).
        m_mem.resize(m_ptr->to()->size());
        m_process.read(m_address, m_mem.data(), m_mem.size());
    }
}
} // namespace node