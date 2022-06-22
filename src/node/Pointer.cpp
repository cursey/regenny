#include <fmt/format.h>
#include <imgui.h>
#include <utf8.h>

#include "Array.hpp"
#include "Struct.hpp"

#include "Pointer.hpp"

using namespace std::literals;

namespace node {
void Pointer::display_str(std::string& s, const std::string& str) {
    s += "\"";

    for (auto&& c : str) {
        if (c == 0) {
            break;
        }

        s += c;
    }

    s += "\" ";
}

Pointer::Pointer(Config& cfg, Process& process, genny::Variable* var, Property& props)
    : Variable{cfg, process, var, props} {
    m_ptr = dynamic_cast<genny::Pointer*>(m_var->type());
    assert(m_ptr != nullptr);

    m_props["__collapsed"].set_default(true);
    m_props["__array"].set_default(false);
    m_props["__count"].set_default(1);

    // Make sure the array count is never < 1 (can happen if the node was previously an array node or a user hand-edited
    // the props).
    if (array_count() < 1) {
        array_count() = 1;
    }
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
        ImGui::TextUnformatted(m_address_str.c_str());
        ImGui::PopStyleColor();

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

        m_is_hovered = ImGui::IsItemHovered();

        if (ImGui::BeginPopupContextItem("PointerNode")) {
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
            m_ptr_node = std::make_unique<Array>(m_cfg, m_process, m_proxy_var.get(), props);
        } else {
            m_proxy_var = std::make_unique<genny::Variable>(var_name);
            m_proxy_var->type(m_ptr->to());

            if (m_ptr->to()->is_a<genny::Struct>()) {
                auto struct_ = std::make_unique<Struct>(m_cfg, m_process, m_proxy_var.get(), props);
                struct_->display_self(false)->is_collapsed(false);
                m_ptr_node = std::move(struct_);
            } else if (m_ptr->to()->is_a<genny::Pointer>()) {
                m_ptr_node = std::make_unique<Pointer>(m_cfg, m_process, m_proxy_var.get(), props);
            } else {
                m_ptr_node = std::make_unique<Variable>(m_cfg, m_process, m_proxy_var.get(), props);
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

void Pointer::update(uintptr_t address, uintptr_t offset, std::byte* mem) {
    Base::update(address, offset, mem);
    m_value_str.clear();
    m_address_str.clear();

    for (auto&& md : m_var->metadata()) {
        if (md == "utf8*") {
            m_utf8.resize(256);
            m_process.read(*(uintptr_t*)mem, m_utf8.data(), 255 * sizeof(char));
            display_str(m_value_str, m_utf8);
        } else if (md == "utf16*") {
            m_utf16.resize(256);
            m_process.read(*(uintptr_t*)mem, m_utf16.data(), 255 * sizeof(char16_t));

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
            m_utf32.resize(256);
            m_process.read(*(uintptr_t*)mem, m_utf32.data(), 255 * sizeof(char32_t));

            std::string utf32conv{};

            try {
                utf32conv = utf8::utf32to8(m_utf32);
            } catch (utf8::invalid_utf16& e) {
                utf32conv = e.what();
            }

            display_str(m_value_str, utf32conv);
        }
    }

    auto addr = *(uintptr_t*)mem;

    // RTTI
    if (auto tn = m_process.get_typename(addr); tn) {
        fmt::format_to(std::back_inserter(m_address_str), "obj*:{:s} ", *tn);
    }

    for (auto&& mod : m_process.modules()) {
        if (mod.start <= addr && addr <= mod.end) {
            fmt::format_to(std::back_inserter(m_address_str), "<{}>+0x{:X}", mod.name, addr - mod.start);
            // Bail here so we don't try previewing this pointer as something else.
            return;
        }
    }

    for (auto&& allocation : m_process.allocations()) {
        if (allocation.start <= addr && addr <= allocation.end) {
            fmt::format_to(std::back_inserter(m_address_str), "0x{:X}", addr);
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
        m_ptr_node->update(m_address, 0, &m_mem[0]);
    }
}
} // namespace node