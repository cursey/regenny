#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Struct : public Variable {
public:
    Struct(Process& process, genny::Variable* var);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;

    auto collapse(bool collapse) {
        m_is_collapsed = collapse;
        return this;
    }
    auto collapse() const { return m_is_collapsed; }

    auto display_self(bool display_self) {
        m_display_self = display_self;
        return this;
    }
    auto display_self() const { return m_display_self; }

private:
    bool m_is_collapsed{true};
    bool m_display_self{true};
    genny::Struct* m_struct{};
    size_t m_size{};
    std::map<uintptr_t, std::unique_ptr<Base>> m_nodes{};
};

} // namespace node