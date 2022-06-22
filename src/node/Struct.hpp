#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Struct : public Variable {
public:
    Struct(Config& cfg, Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto is_collapsed(bool is_collapsed) {
        m_props["__collapsed"].set(is_collapsed);
        return this;
    }
    auto& is_collapsed() { return m_props["__collapsed"].as_bool(); }

    auto display_self(bool display_self) {
        m_display_self = display_self;
        return this;
    }
    auto display_self() const { return m_display_self; }

private:
    bool m_display_self{true};
    genny::Struct* m_struct{};
    std::multimap<uintptr_t, std::unique_ptr<Base>> m_nodes{};
    bool m_is_hovered{};
    std::string m_display_str{};

    void fill_space(uintptr_t last_offset, int delta);
};

} // namespace node