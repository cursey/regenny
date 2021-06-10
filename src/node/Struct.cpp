#include <imgui.h>

#include "Array.hpp"

#include "Struct.hpp"

namespace node {
Struct::Struct(genny::Variable* var)
    : Variable{var}, m_struct{dynamic_cast<genny::Struct*>(var->type())}, m_size{var->size()} {
    assert(m_struct != nullptr);

    // Build the node map.
    for (auto&& var : m_struct->get_all<genny::Variable>()) {
        std::unique_ptr<Variable> node{};

        if (auto arr = dynamic_cast<genny::Array*>(var)) {
            node = std::make_unique<Array>(arr);
        } else if (var->type()->is_a<genny::Struct>()) {
            node = std::make_unique<Struct>(var);
        } else {
            node = std::make_unique<Variable>(var);
        }

        m_nodes[var->offset()] = std::move(node);
    }
}

void Struct::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
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

    for (auto offset = 0; offset < m_struct->size();) {
        if (auto search = m_nodes.find(offset); search != m_nodes.end()) {
            auto& node = search->second;

            ++indentation_level;
            ImGui::PushID(node.get());
            node->display(address + offset, offset, &mem[offset]);
            ImGui::PopID();
            --indentation_level;

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
