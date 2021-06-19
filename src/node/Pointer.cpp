#include <imgui.h>

#include "Array.hpp"
#include "Struct.hpp"

#include "Pointer.hpp"

using namespace std::literals;

namespace node {
Pointer::Pointer(Process& process, genny::Variable* var) : Variable{process, var} {
    m_ptr = dynamic_cast<genny::Pointer*>(m_var->type());
    assert(m_ptr != nullptr);
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

        if (ImGui::BeginPopupContextItem("PointerNode")) {
            ImGui::Checkbox("Collpase", &m_is_collapsed);

            if (ImGui::Checkbox("Is array", &m_is_array)) {
                m_ptr_node = nullptr;
            }

            if (m_is_array) {
                if (ImGui::InputInt("Array count", &m_count)) {
                    if (m_count < 1) {
                        m_count = 1;
                    }

                    m_ptr_node = nullptr;
                }
            }

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

    // We create the node here right before displaying it to avoid pointer loop crashes. Only nodes that are uncollapsed
    // get created.
    if (m_ptr_node == nullptr) {
        if (m_is_array) {
            m_proxy_var = std::make_unique<genny::Variable>(m_var->name());
            m_proxy_var->type(m_ptr->to()->array_(m_count));
            m_ptr_node = std::make_unique<Array>(m_process, m_proxy_var.get());
        } else {
            m_proxy_var = std::make_unique<genny::Variable>(m_var->name());
            m_proxy_var->type(m_ptr->to());

            if (m_ptr->to()->is_a<genny::Struct>()) {
                auto struct_ = std::make_unique<Struct>(m_process, m_proxy_var.get());
                struct_->display_self(false)->collapse(false);
                m_ptr_node = std::move(struct_);
            } else if (m_ptr->to()->is_a<genny::Pointer>()) {
                m_ptr_node = std::make_unique<Pointer>(m_process, m_proxy_var.get());
            } else {
                m_ptr_node = std::make_unique<Variable>(m_process, m_proxy_var.get());
            }
        }
    }

    refresh_memory();

    // This can happen if the type pointed to is empty. For example if the user has just created the type in the editor
    // and the memory ui has been refreshed.
    if (m_mem.empty()) {
        return;
    }

    ++indentation_level;
    ImGui::PushID(m_ptr_node.get());
    m_ptr_node->display(m_address, 0, &m_mem[0]);
    ImGui::PopID();
    --indentation_level;
}

void Pointer::refresh_memory() {
    if (m_is_collapsed) {
        return;
    }

    if (auto now = std::chrono::steady_clock::now(); now >= m_mem_refresh_time) {
        m_mem_refresh_time = now + 500ms;

        // Make sure our memory buffer is large enough (since the first refresh it wont be).
        m_mem.resize(m_ptr->to()->size() * m_count);
        m_process.read(m_address, m_mem.data(), m_mem.size());
        m_ptr_node->on_refresh(m_address, 0, &m_mem[0]);
    }
}
} // namespace node