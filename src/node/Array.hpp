#pragma once

#include "Variable.hpp"

namespace node {
class Array : public Variable {
public:
    Array(Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto start_element(int start_element) {
        m_props["__start"].set(start_element);
        return this;
    }
    auto& start_element() { return m_props["__start"].as_int(); }

    auto num_elements_displayed(int num_elements) {
        m_props["__count"].set(num_elements);
        return this;
    }
    auto& num_elements_displayed() { return m_props["__count"].as_int(); }

protected:
    genny::Array* m_arr{};
    std::vector<std::unique_ptr<Variable>> m_elements{};
    std::vector<std::unique_ptr<genny::Variable>> m_proxy_variables{};

    void create_nodes();
};
} // namespace node