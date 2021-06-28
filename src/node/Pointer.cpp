#include <fmt/format.h>
#include <imgui.h>

#include "Array.hpp"
#include "Struct.hpp"

#include "Pointer.hpp"

using namespace std::literals;

namespace node {
Pointer::Pointer(Process& process, genny::Variable* var, Property& props) : Variable{process, var, props} {
    m_ptr = dynamic_cast<genny::Pointer*>(m_var->type());
    assert(m_ptr != nullptr);

    m_props["__collapsed"].set_default(true);
    m_props["__array"].set_default(false);
    m_props["__count"].set_default(1);
}

void Pointer::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (indentation_level >= 0) {
        display_address_offset(address, offset);
        ImGui::SameLine();
        ImGui::BeginGroup();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::SameLine();
        // ImGui::Text("%p", *(uintptr_t*)mem);
        ImGui::PushStyleColor(ImGuiCol_Text, {0.6f, 0.6f, 0.6f, 1.0f});
        ImGui::TextUnformatted(m_value_str.c_str());
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        m_is_hovered = ImGui::IsItemHovered();

        if (ImGui::BeginPopupContextItem("PointerNode")) {
            ImGui::Checkbox("Collpase", &is_collapsed());

            if (ImGui::Checkbox("Is Array", &is_array())) {
                m_ptr_node = nullptr;
            }

            if (is_array()) {
                if (ImGui::InputInt("Array Count", &array_count())) {
                    if (array_count() < 1) {
                        array_count() = 1;
                    }

                    m_ptr_node = nullptr;
                }
            }

            ImGui::EndPopup();
        }

        if (is_collapsed() && !m_is_hovered) {
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
        auto&& var_name = m_var->name();
        auto&& props = m_props[var_name];

        if (is_array()) {
            m_proxy_var = std::make_unique<genny::Variable>(var_name);
            m_proxy_var->type(m_ptr->to()->array_(array_count()));
            m_ptr_node = std::make_unique<Array>(m_process, m_proxy_var.get(), props);
        } else {
            m_proxy_var = std::make_unique<genny::Variable>(var_name);
            m_proxy_var->type(m_ptr->to());

            if (m_ptr->to()->is_a<genny::Struct>()) {
                auto struct_ = std::make_unique<Struct>(m_process, m_proxy_var.get(), props);
                struct_->display_self(false)->is_collapsed(false);
                m_ptr_node = std::move(struct_);
            } else if (m_ptr->to()->is_a<genny::Pointer>()) {
                m_ptr_node = std::make_unique<Pointer>(m_process, m_proxy_var.get(), props);
            } else {
                m_ptr_node = std::make_unique<Variable>(m_process, m_proxy_var.get(), props);
            }
        }
    }

    refresh_memory();

    // This can happen if the type pointed to is empty. For example if the user has just created the type in the editor
    // and the memory ui has been refreshed.
    if (m_mem.empty()) {
        return;
    }

    auto show_tooltip = is_collapsed() && m_is_hovered;
    auto backup_indentation_level = indentation_level;

    if (show_tooltip) {
        ImGui::BeginTooltip();
        indentation_level = 0;
    } else {
        ++indentation_level;
    }

    ImGui::PushID(m_ptr_node.get());
    m_ptr_node->display(m_address, 0, &m_mem[0]);
    ImGui::PopID();

    if (show_tooltip) {
        ImGui::EndTooltip();
    }

    indentation_level = backup_indentation_level;
}

void Pointer::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::on_refresh(address, offset, mem);
    m_value_str.clear();

    auto addr = *(uintptr_t*)mem;

    for (auto&& mod : m_process.modules()) {
        if (mod.start <= addr && addr <= mod.end) {
            fmt::format_to(std::back_inserter(m_value_str), "<{}>+0x{:X}", mod.name, addr - mod.start);
            // Bail here so we don't try previewing this pointer as something else.
            return;
        }
    }

    for (auto&& allocation : m_process.allocations()) {
        if (allocation.start <= addr && addr <= allocation.end) {
            fmt::format_to(std::back_inserter(m_value_str), "0x{:X}", addr);
            // Bail here so we don't try previewing this pointer as something else.
            return;
        }
    }
}

void Pointer::refresh_memory() {
    if ((is_collapsed() && !m_is_hovered) || m_ptr->to()->size() == 0) {
        return;
    }

    if (auto now = std::chrono::steady_clock::now(); now >= m_mem_refresh_time) {
        m_mem_refresh_time = now + 500ms;

        // Make sure our memory buffer is large enough (since the first refresh it wont be).
        m_mem.resize(m_ptr->to()->size() * array_count());
        m_process.read(m_address, m_mem.data(), m_mem.size());
        m_ptr_node->on_refresh(m_address, 0, &m_mem[0]);
    }
}
} // namespace node