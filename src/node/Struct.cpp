#include <imgui.h>

#include "Array.hpp"
#include "Pointer.hpp"

#include "Struct.hpp"

namespace node {
Struct::Struct(Process& process, genny::Variable* var)
    : Variable{process, var}, m_struct{dynamic_cast<genny::Struct*>(var->type())}, m_size{var->size()} {
    assert(m_struct != nullptr);

    // Build the node map.
    for (auto&& var : m_struct->get_all<genny::Variable>()) {
        std::unique_ptr<Variable> node{};

        if (var->type()->is_a<genny::Array>()) {
            node = std::make_unique<Array>(m_process, var);
        } else if (var->type()->is_a<genny::Struct>()) {
            node = std::make_unique<Struct>(m_process, var);
        } else if (var->type()->is_a<genny::Pointer>()) {
            node = std::make_unique<Pointer>(m_process, var);
        } else {
            node = std::make_unique<Variable>(m_process, var);
        }

        m_nodes[var->offset()] = std::move(node);
    }
}

void Struct::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (m_display_self) {
        ImGui::BeginGroup();
        display_address_offset(address, offset);
        ImGui::SameLine();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::EndGroup();

        if (ImGui::BeginPopupContextItem("ArrayNode")) {
            ImGui::Checkbox("Collpase", &m_is_collapsed);
            ImGui::EndPopup();
        }

        if (m_is_collapsed) {
            return;
        }
    }

    for (auto offset = 0; offset < m_struct->size();) {
        if (auto search = m_nodes.find(offset); search != m_nodes.end()) {
            auto& node = search->second;

            if (m_display_self) {
                ++indentation_level;
            }

            ImGui::PushID(node.get());
            node->display(address + offset, offset, &mem[offset]);
            ImGui::PopID();

            if (m_display_self) {
                --indentation_level;
            }

            offset += node->size();
        } else {
            ++offset;
        }
    }
}

size_t Struct::size() {
    return m_size;
}
} // namespace node
