#include <array>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "MemoryUi.hpp"

using namespace std::literals;

template <typename T> void display_as(std::string& s, std::byte* mem) {
    fmt::format_to(std::back_inserter(s), "{} ", *(T*)mem);
}

int g_indent{};

class VariableNode : public MemoryUi::Node {
public:
    VariableNode(genny::Variable* var) : m_var{var} {}

    virtual void display_type() { ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, m_var->type()->name().c_str()); }

    virtual void display_name() { ImGui::Text("%s", m_var->name().c_str()); }

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override {
        display_address_offset(address, offset);
        ImGui::SameLine();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::SameLine();

        static std::string s{};

        s.clear();

        auto end = std::min(m_var->size(), sizeof(uintptr_t));

        for (auto i = 0; i < end; ++i) {
            fmt::format_to(std::back_inserter(s), "{:02X}", *(uint8_t*)&mem[i]);
        }

        if (end < m_var->size()) {
            s += "...";
        }

        s += ' ';

        std::array<std::vector<std::string>*, 2> metadatas{&m_var->metadata(), &m_var->type()->metadata()};

        for (auto&& metadata : metadatas) {
            for (auto&& md : *metadata) {
                if (md == "u8") {
                    display_as<uint8_t>(s, mem);
                } else if (md == "u16") {
                    display_as<uint16_t>(s, mem);
                } else if (md == "u32") {
                    display_as<uint32_t>(s, mem);
                } else if (md == "u64") {
                    display_as<uint64_t>(s, mem);
                } else if (md == "i8") {
                    display_as<int8_t>(s, mem);
                } else if (md == "i16") {
                    display_as<int16_t>(s, mem);
                } else if (md == "i32") {
                    display_as<int32_t>(s, mem);
                } else if (md == "i64") {
                    display_as<int64_t>(s, mem);
                } else if (md == "f32") {
                    display_as<float>(s, mem);
                } else if (md == "f64") {
                    display_as<double>(s, mem);
                }
            }
        }

        ImGui::TextUnformatted(s.c_str());
    }

    size_t size() override { return m_var->size(); }

protected:
    genny::Variable* m_var{};
};

class StructNode;

class ArrayNode : public VariableNode {
public:
    ArrayNode(genny::Array* arr);

    void display_type() override {
        auto g = ImGui::GetCurrentContext();
        ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, m_var->type()->name().c_str());
        ImGui::SameLine(0, 0.0);
        ImGui::TextColored({0.4f, 0.4f, 0.8f, 1.0f}, "[%d]", m_arr->count());
    }

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override {
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

            ++g_indent;
            ImGui::PushID(cur_node.get());
            cur_node->display(address + m_cur_element * m_arr->type()->size(),
                offset + m_cur_element * m_arr->type()->size(), mem + m_cur_element * m_arr->type()->size());
            ImGui::PopID();
            --g_indent;
        }
    }

protected:
    genny::Array* m_arr{};
    int m_cur_element{-1};
    std::vector<std::unique_ptr<VariableNode>> m_elements{};
    std::vector<std::unique_ptr<genny::Variable>> m_proxy_variables{};
};

class StructNode : public VariableNode {
public:
    StructNode(genny::Variable* var)
        : VariableNode{var}, m_struct{dynamic_cast<genny::Struct*>(var->type())}, m_size{var->size()} {
        assert(m_struct != nullptr);

        // Build the node map.
        for (auto&& var : m_struct->get_all<genny::Variable>()) {
            std::unique_ptr<VariableNode> node{};

            if (auto arr = dynamic_cast<genny::Array*>(var)) {
                node = std::make_unique<ArrayNode>(arr);
            } else if (var->type()->is_a<genny::Struct>()) {
                node = std::make_unique<StructNode>(var);
            } else {
                node = std::make_unique<VariableNode>(var);
            }

            m_nodes[var->offset()] = std::move(node);
        }
    }

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override {
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

                ++g_indent;
                ImGui::PushID(node.get());
                node->display(address + offset, offset, &mem[offset]);
                ImGui::PopID();
                --g_indent;

                offset += node->size();
            } else {
                ++offset;
            }
        }
    }

    size_t size() override { return m_size; }
    void collapse(bool collapse) { m_is_collapsed = collapse; }
    auto collapse() const { return m_is_collapsed; }

private:
    bool m_is_collapsed{true};
    genny::Struct* m_struct{};
    size_t m_size{};
    std::map<uintptr_t, std::unique_ptr<Node>> m_nodes{};
};

ArrayNode::ArrayNode(genny::Array* arr) : VariableNode{arr}, m_arr{arr} {
    for (auto i = 0; i < m_arr->count(); ++i) {
        auto proxy_variable = std::make_unique<genny::Variable>(fmt::format("{}[{}]", m_arr->name(), i));

        proxy_variable->type(m_arr->type());
        proxy_variable->offset(m_arr->offset() + i * m_arr->type()->size());

        std::unique_ptr<VariableNode> node{};

        if (proxy_variable->type()->is_a<genny::Struct>()) {
            node = std::make_unique<StructNode>(proxy_variable.get());
        } else {
            node = std::make_unique<VariableNode>(proxy_variable.get());
        }

        m_proxy_variables.emplace_back(std::move(proxy_variable));
        m_elements.emplace_back(std::move(node));
    }
}

MemoryUi::MemoryUi(genny::Sdk& sdk, genny::Struct* struct_, Process& process, uintptr_t address)
    : m_sdk{sdk}, m_struct{struct_}, m_process{process}, m_address{address} {
    if (m_struct == nullptr) {
        return;
    }

    /*// Build the node map.
    for (auto&& var : m_struct->get_all<genny::Variable>()) {
        std::unique_ptr<VariableNode> node{};

        if (auto arr = dynamic_cast<genny::Array*>(var)) {
            node = std::make_unique<ArrayNode>(arr);
        } else {
            node = std::make_unique<VariableNode>(var);
        }

        m_nodes[m_address + var->offset()] = std::move(node);
    }*/

    m_proxy_variable = std::make_unique<genny::Variable>("");
    m_proxy_variable->type(m_struct);

    auto root = std::make_unique<StructNode>(m_proxy_variable.get());
    root->collapse(false);

    m_root = std::move(root);
}

void MemoryUi::display() {
    refresh_memory();

    m_root->display(m_address, 0, &m_mem[0]);
    /*for (auto p = m_address; p < m_address + m_mem.size();) {
        if (auto search = m_nodes.find(p); search != m_nodes.end()) {
            auto& node = search->second;

            ImGui::PushID(node.get());
            node->display(p, p - m_address, &m_mem[p - m_address]);
            ImGui::PopID();

            p += node->size();
        } else {
            ++p;
        }
    }*/
}

void MemoryUi::refresh_memory() {
    if (auto now = std::chrono::steady_clock::now(); now >= m_mem_refresh_time) {
        m_mem_refresh_time = now + 500ms;

        // Make sure our memory buffer is large enough (since the first refresh it wont be).
        m_mem.resize(m_struct->size());
        m_process.read(m_address, m_mem.data(), m_mem.size());
    }
}

void MemoryUi::Node::display_address_offset(uintptr_t address, uintptr_t offset) {
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%016llX", address);
    ImGui::SameLine();
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%8X", offset);

    if (g_indent > 0) {
        auto g = ImGui::GetCurrentContext();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2{g->Style.IndentSpacing * g_indent, g->FontSize});
    }
}
