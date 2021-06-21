#include <fmt/format.h>
#include <imgui.h>

#include "Pointer.hpp"
#include "Struct.hpp"

#include "Array.hpp"

namespace node {
Array::Array(Process& process, genny::Variable* var, Property& props)
    : Variable{process, var, props}, m_arr{dynamic_cast<genny::Array*>(var->type())} {
    assert(m_arr != nullptr);

    if (m_props["__start"].value.index() == 0) {
        start_element(0);
    }

    if (m_props["__count"].value.index() == 0) {
        num_elements_displayed(0);
    }

    // Make sure inherited props are within acceptable ranges.
    start_element() = std::clamp(start_element(), 0, (int)m_arr->count());
    num_elements_displayed() = std::clamp(num_elements_displayed(), 0, (int)m_arr->count());

    // Create the initial nodes.
    create_nodes();
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
        if (ImGui::InputInt("Start element", &start_element())) {
            start_element() = std::clamp(start_element(), 0, (int)m_arr->count());
            create_nodes();
        }

        if (ImGui::InputInt("# Elements displayed", &num_elements_displayed())) {
            num_elements_displayed() = std::clamp(num_elements_displayed(), 0, (int)m_arr->count());
            create_nodes();
        }

        ImGui::EndPopup();
    }

    auto start = start_element();
    auto num_elements = num_elements_displayed();

    for (auto i = 0; i < num_elements; ++i) {
        auto cur_element = start + i;
        auto& cur_node = m_elements[i];
        auto cur_offset = cur_element * m_arr->of()->size();

        ++indentation_level;
        ImGui::PushID(cur_node.get());
        cur_node->display(address + cur_offset, offset + cur_offset, mem + cur_offset);
        ImGui::PopID();
        --indentation_level;
    }
}

void Array::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    auto num_elements = num_elements_displayed();
    auto start = start_element();

    for (auto i = 0; i < num_elements; ++i) {
        auto cur_element = start + i;
        auto& cur_node = m_elements[i];
        auto cur_offset = cur_element * m_arr->of()->size();

        cur_node->on_refresh(address + cur_offset, offset + cur_offset, mem + cur_offset);
    }
}

void Array::create_nodes() {
    m_proxy_variables.clear();
    m_elements.clear();

    auto num_elements = num_elements_displayed();
    auto start = start_element();
    auto end = start + num_elements;

    for (auto i = start; i < end; ++i) {
        auto proxy_variable = std::make_unique<genny::Variable>(fmt::format("{}[{}]", m_var->name(), i));
        auto&& proxy_props = m_props[proxy_variable->name()];

        proxy_variable->type(m_arr->of());
        proxy_variable->offset(m_var->offset() + i * m_arr->size());

        std::unique_ptr<Variable> node{};

        if (proxy_variable->type()->is_a<genny::Array>()) {
            node = std::make_unique<Array>(m_process, proxy_variable.get(), proxy_props);
        } else if (proxy_variable->type()->is_a<genny::Struct>()) {
            auto struct_ = std::make_unique<Struct>(m_process, proxy_variable.get(), proxy_props);
            struct_->is_collapsed(false);
            node = std::move(struct_);
        } else if (proxy_variable->type()->is_a<genny::Pointer>()) {
            node = std::make_unique<Pointer>(m_process, proxy_variable.get(), proxy_props);
        } else {
            node = std::make_unique<Variable>(m_process, proxy_variable.get(), proxy_props);
        }

        m_proxy_variables.emplace_back(std::move(proxy_variable));
        m_elements.emplace_back(std::move(node));
    }
}
} // namespace node