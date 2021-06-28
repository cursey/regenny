#include <fmt/format.h>
#include <imgui.h>

#include "Array.hpp"
#include "Bitfield.hpp"
#include "Pointer.hpp"
#include "Undefined.hpp"

#include "Struct.hpp"

namespace node {
Struct::Struct(Process& process, genny::Variable* var, Property& props)
    : Variable{process, var, props}, m_struct{dynamic_cast<genny::Struct*>(var->type())} {
    assert(m_struct != nullptr);

    m_props["__collapsed"].set_default(true);

    // Build the node map.
    auto make_node = [&](genny::Variable* var) -> std::unique_ptr<Base> {
        auto&& props = m_props[var->name()];

        if (var->type()->is_a<genny::Array>()) {
            return std::make_unique<Array>(m_process, var, props);
        } else if (var->type()->is_a<genny::Struct>()) {
            return std::make_unique<Struct>(m_process, var, props);
        } else if (var->type()->is_a<genny::Pointer>()) {
            return std::make_unique<Pointer>(m_process, var, props);
        } else if (var->is_bitfield()) {
            return std::make_unique<Bitfield>(m_process, var, props);
        } else {
            return std::make_unique<Variable>(m_process, var, props);
        }
    };
    std::function<void(genny::Struct*)> add_vars = [&](genny::Struct* s) {
        for (auto&& parent : s->parents()) {
            add_vars(parent);
        }

        for (auto&& var : s->get_all<genny::Variable>()) {
            m_nodes.emplace(var->offset(), make_node(var));
        }
    };

    add_vars(m_struct);

    // Fill in the rest of the offsets with undefined nodes.
    if (!m_nodes.empty()) {
        for (auto i = m_nodes.begin(), j = std::next(m_nodes.begin()); j != m_nodes.end(); i = j++) {
            auto last_offset = i->first + i->second->size();
            auto delta = j->first - last_offset;

            fill_space(last_offset, delta);
        }

        // Fill in the end.
        auto last_node = m_nodes.rbegin();
        auto last_offset = last_node->first + last_node->second->size();
        auto delta = m_size - last_offset;

        fill_space(last_offset, delta);
    } else {
        fill_space(0, m_size);
    }
}

void Struct::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (m_display_self) {
        display_address_offset(address, offset);
        ImGui::SameLine();
        ImGui::BeginGroup();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::EndGroup();

        m_is_hovered = ImGui::IsItemHovered();

        if (ImGui::BeginPopupContextItem("ArrayNode")) {
            ImGui::Checkbox("Collpase", &is_collapsed());
            ImGui::EndPopup();
        }

        if (is_collapsed() && !m_is_hovered) {
            return;
        }
    }

    auto show_tooltip = is_collapsed() && m_is_hovered;

    if (show_tooltip) {
        ImGui::BeginTooltip();
    }

    auto it = m_nodes.begin();

    for (uintptr_t node_offset = 0; node_offset < m_size;) {
        // Advance the iterator until the next node >= the current node_offset.
        for (; it != m_nodes.end() && it->first < node_offset; ++it) {
        }

        if (it == m_nodes.end()) {
            fill_space(node_offset, m_size - node_offset);
            it = m_nodes.find(node_offset);
        }

        if (node_offset != it->first) {
            // Advance until the next non-undefined node.
            for (; it != m_nodes.end() && dynamic_cast<Undefined*>(it->second.get()); ++it) {
            }

            // Fill in the space.
            if (it != m_nodes.end()) {
                auto delta = it->first - node_offset;
                fill_space(node_offset, delta);
            } else {
                fill_space(node_offset, m_size - node_offset);
            }

            // There will now be a node where @ node_offset. 
            it = m_nodes.find(node_offset);
        }

        auto backup_indentation_level = indentation_level;

        if (show_tooltip) {
            indentation_level = 0;
        } else if (m_display_self) {
            ++indentation_level;
        }

        auto& node = it->second;

        ImGui::PushID(node.get());
        node->display(address + node_offset, offset + node_offset, &mem[node_offset]);
        ImGui::PopID();

        indentation_level = backup_indentation_level;

        node_offset += node->size();
    }

    if (show_tooltip) {
        ImGui::EndTooltip();
    }
}

void Struct::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::on_refresh(address, offset, mem);

    if (is_collapsed() && !m_is_hovered) {
        return;
    }

    for (auto&& [node_offset, node] : m_nodes) {
        node->on_refresh(address + node_offset, offset + node_offset, &mem[node_offset]);
    }
}

void Struct::fill_space(uintptr_t last_offset, int delta) {
    auto add_undefined = [this](int offset, int size) {
        auto& props = m_props[fmt::format("undefined_{:x}", offset)];
        m_nodes.emplace(offset, std::make_unique<Undefined>(m_process, props, size));

        // Delete nodes that are overwritten by the undefined node we just added.
        for (auto i = offset + 1; i < offset + size; ++i) {
            m_nodes.erase(i);
        }
    };

    auto start = last_offset;
    auto end = last_offset + delta;

    for (auto offset = start; offset <= end; ++offset) {
        if (offset % sizeof(uintptr_t) == 0 || offset == end) {
            switch (offset - last_offset) {
            case 8:
                add_undefined(last_offset, 8);
                break;

            case 7:
                add_undefined(last_offset, 4);
                add_undefined(last_offset + 4, 2);
                add_undefined(last_offset + 6, 1);
                break;

            case 6:
                add_undefined(last_offset, 4);
                add_undefined(last_offset + 4, 2);
                break;

            case 5:
                add_undefined(last_offset, 4);
                add_undefined(last_offset + 4, 1);
                break;

            case 4:
                add_undefined(last_offset, 4);
                break;

            case 3:
                add_undefined(last_offset, 2);
                add_undefined(last_offset + 2, 1);
                break;

            case 2:
                add_undefined(last_offset, 2);
                break;

            case 1:
                add_undefined(last_offset, 1);
                break;

            default:
                break;
            }

            last_offset = offset;
        }
    }
}
} // namespace node
