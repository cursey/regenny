#include <fmt/format.h>
#include <imgui.h>

#include "Struct.hpp"

#include "Array.hpp"

namespace node {
Array::Array(genny::Array* arr) : Variable{arr}, m_arr{arr} {
    for (auto i = 0; i < m_arr->count(); ++i) {
        auto proxy_variable = std::make_unique<genny::Variable>(fmt::format("{}[{}]", m_arr->name(), i));

        proxy_variable->type(m_arr->type());
        proxy_variable->offset(m_arr->offset() + i * m_arr->type()->size());

        std::unique_ptr<Variable> node{};

        if (proxy_variable->type()->is_a<genny::Struct>()) {
            node = std::make_unique<Struct>(proxy_variable.get());
        } else {
            node = std::make_unique<Variable>(proxy_variable.get());
        }

        m_proxy_variables.emplace_back(std::move(proxy_variable));
        m_elements.emplace_back(std::move(node));
    }
}

void Array::display_type() {
    auto g = ImGui::GetCurrentContext();
    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, m_var->type()->name().c_str());
    ImGui::SameLine(0, 0.0);
    ImGui::TextColored({0.4f, 0.4f, 0.8f, 1.0f}, "[%d]", m_arr->count());
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
        if (ImGui::InputInt("Display element", &m_cur_element, 1, 100)) {
            m_cur_element = std::clamp(m_cur_element, -1, (int)m_elements.size() - 1);
        }

        ImGui::EndPopup();
    }

    if (m_cur_element != -1) {
        auto& cur_node = m_elements[m_cur_element];

        ++indentation_level;
        ImGui::PushID(cur_node.get());
        cur_node->display(address + m_cur_element * m_arr->type()->size(),
            offset + m_cur_element * m_arr->type()->size(), mem + m_cur_element * m_arr->type()->size());
        ImGui::PopID();
        --indentation_level;
    }
}
} // namespace node