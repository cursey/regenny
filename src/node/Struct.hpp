#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Struct : public Variable {
public:
    Struct(Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto is_collapsed(bool is_collapsed) {
        m_props["__collapsed"].value = is_collapsed;
        return this;
    }
    auto& is_collapsed() { return std::get<bool>(m_props["__collapsed"].value); }

    auto display_self(bool display_self) {
        m_display_self = display_self;
        return this;
    }
    auto display_self() const { return m_display_self; }

private:
    bool m_display_self{true};
    genny::Struct* m_struct{};
    std::map<uintptr_t, std::unique_ptr<Base>> m_nodes{};
};

} // namespace node