#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Struct : public Variable {
public:
    Struct(genny::Variable* var);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;

    void collapse(bool collapse) { m_is_collapsed = collapse; }
    auto collapse() const { return m_is_collapsed; }

private:
    bool m_is_collapsed{true};
    genny::Struct* m_struct{};
    size_t m_size{};
    std::map<uintptr_t, std::unique_ptr<Base>> m_nodes{};
};

} // namespace node