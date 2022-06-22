#include <fmt/format.h>
#include <imgui.h>
#include <utf8.h>

#include "Pointer.hpp"
#include "Struct.hpp"

#include "Array.hpp"

namespace node {
void Array::display_str(std::string& s, const std::string& str) {
    s += "\"";

    for (auto&& c : str) {
        if (c == 0) {
            break;
        }

        s += c;
    }

    s += "\" ";
}

Array::Array(Config& cfg, Process& process, genny::Variable* var, Property& props)
    : Variable{cfg, process, var, props}, m_arr{dynamic_cast<genny::Array*>(var->type())} {
    assert(m_arr != nullptr);

    m_props["__collapsed"].set_default(true);
    m_props["__start"].set_default(0);
    m_props["__count"].set_default(std::min(10, (int)m_arr->count()));

    // Make sure inherited props are within acceptable ranges.
    start_element() = std::clamp(start_element(), 0, (int)m_arr->count());
    num_elements_displayed() = std::clamp(num_elements_displayed(), 0, (int)m_arr->count());

    // Create the initial nodes.
    create_nodes();
}

void Array::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();
    ImGui::BeginGroup();
    display_type();
    ImGui::SameLine();
    display_name();

    if (!m_value_str.empty()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, {181.0f / 255.0f, 206.0f / 255.0f, 168.0f / 255.0f, 1.0f});
        ImGui::TextUnformatted(m_value_str.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();

    if (ImGui::IsItemClicked()) {
        is_collapsed() = !is_collapsed();
    }

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

    if (is_collapsed()) {
        return;
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

void Array::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::update(address, offset, mem);
    m_value_str.clear();

    auto num_elements = num_elements_displayed();
    auto start = start_element();

    for (auto i = 0; i < num_elements; ++i) {
        auto cur_element = start + i;
        auto& cur_node = m_elements[i];
        auto cur_offset = cur_element * m_arr->of()->size();

        cur_node->update(address + cur_offset, offset + cur_offset, mem + cur_offset);
    }

    for (auto&& md : m_var->metadata()) {
        if (md == "utf8*") {
            m_utf8.resize(m_arr->count());
            memcpy(m_utf8.data(), mem, m_arr->count() * sizeof(char));
            display_str(m_value_str, m_utf8);
        } else if (md == "utf16*") {
            m_utf16.resize(m_arr->count());
            memcpy(m_utf16.data(), mem, m_arr->count() * sizeof(char16_t));

            m_utf16.back() = L'\0';

            // if we don't do this then utf16to8 will throw an exception.
            // todo: do for utf32?
            const auto real_len = wcslen((wchar_t*)m_utf16.data());
            m_utf16.resize(real_len);

            std::string utf8conv{};

            try {
                utf8conv = utf8::utf16to8(m_utf16);
            } catch (utf8::invalid_utf16& e) {
                utf8conv = e.what();
            }

            display_str(m_value_str, utf8conv);
        } else if (md == "utf32*") {
            m_utf32.resize(m_arr->count());
            memcpy(m_utf32.data(), mem, m_arr->count() * sizeof(char32_t));

            std::string utf32conv{};

            try {
                utf32conv = utf8::utf32to8(m_utf32);
            } catch (utf8::invalid_utf16& e) {
                utf32conv = e.what();
            }

            display_str(m_value_str, utf32conv);
        }
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
            node = std::make_unique<Array>(m_cfg, m_process, proxy_variable.get(), proxy_props);
        } else if (proxy_variable->type()->is_a<genny::Struct>()) {
            auto struct_ = std::make_unique<Struct>(m_cfg, m_process, proxy_variable.get(), proxy_props);
            struct_->is_collapsed(false);
            node = std::move(struct_);
        } else if (proxy_variable->type()->is_a<genny::Pointer>()) {
            node = std::make_unique<Pointer>(m_cfg, m_process, proxy_variable.get(), proxy_props);
        } else {
            node = std::make_unique<Variable>(m_cfg, m_process, proxy_variable.get(), proxy_props);
        }

        m_proxy_variables.emplace_back(std::move(proxy_variable));
        m_elements.emplace_back(std::move(node));
    }
}
} // namespace node