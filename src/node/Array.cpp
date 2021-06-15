#include <fmt/format.h>
#include <imgui.h>

#include "Pointer.hpp"
#include "Struct.hpp"

#include "Array.hpp"

namespace node {
Array::Array(Process& process, genny::Variable* var)
    : Variable{process, var}, m_arr{dynamic_cast<genny::Array*>(var->type())} {
    assert(m_arr != nullptr);

    for (auto i = 0; i < m_arr->count(); ++i) {
        auto proxy_variable = std::make_unique<genny::Variable>(fmt::format("{}[{}]", m_var->name(), i));

        proxy_variable->type(m_arr->of());
        proxy_variable->offset(m_var->offset() + i * m_arr->size());

        std::unique_ptr<Variable> node{};

        if (proxy_variable->type()->is_a<genny::Array>()) {
            node = std::make_unique<Array>(m_process, proxy_variable.get());
        } else if (proxy_variable->type()->is_a<genny::Struct>()) {
            auto struct_ = std::make_unique<Struct>(m_process, proxy_variable.get());
            struct_->display_self(false);
            node = std::move(struct_);
        } else if (proxy_variable->type()->is_a<genny::Pointer>()) {
            node = std::make_unique<Pointer>(m_process, proxy_variable.get());
        } else {
            node = std::make_unique<Variable>(m_process, proxy_variable.get());
        }

        m_proxy_variables.emplace_back(std::move(proxy_variable));
        m_elements.emplace_back(std::move(node));
    }
}

void Array::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    ImGui::BeginGroup();
    display_address_offset(address, offset);
    ImGui::SameLine();
    display_type();
    ImGui::SameLine();
    display_name();
    ImGui::EndGroup();

    if (ImGui::BeginPopupContextItem("ArrayNode")) {
        if (ImGui::InputInt("Start element", &m_start_element)) {
            m_start_element = std::clamp(m_start_element, 0, (int)m_elements.size() - 1);
        }

        if (ImGui::InputInt("# Elements displayed", &m_num_elements_displayed)) {
            m_num_elements_displayed = std::clamp(m_num_elements_displayed, 0, (int)m_elements.size());
        }

        ImGui::EndPopup();
    }

    for (auto i = 0; i < m_num_elements_displayed; ++i) {
        if (m_start_element + i >= (int)m_arr->count()) {
            break;
        }

        auto cur_element = m_start_element + i;
        auto& cur_node = m_elements[cur_element];
        auto cur_offset = cur_element * m_arr->of()->size();

        ++indentation_level;
        ImGui::PushID(cur_node.get());
        cur_node->display(address + cur_offset, offset + cur_offset, mem + cur_offset);
        ImGui::PopID();
        --indentation_level;
    }
}

void Array::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    for (auto i = 0; i < m_num_elements_displayed; ++i) {
        if (m_start_element + i >= (int)m_arr->count()) {
            break;
        }

        auto cur_element = m_start_element + i;
        auto& cur_node = m_elements[cur_element];
        auto cur_offset = cur_element * m_arr->of()->size();

        cur_node->on_refresh(address + cur_offset, offset + cur_offset, mem + cur_offset);
    }
}
} // namespace node