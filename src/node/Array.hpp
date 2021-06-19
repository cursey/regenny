#pragma once

#include "Variable.hpp"

namespace node {
class Array : public Variable {
public:
    Array(Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto start_element(int start_element) { 
        m_props["__start"].value = start_element;
        return this;
    }
    auto& start_element() { return std::get<int>(m_props["__start"].value); }

    auto num_elements_displayed(int num_elements) {
        m_props["__count"].value = num_elements;
        return this;
    }
    auto& num_elements_displayed() { return std::get<int>(m_props["__count"].value); }


protected:
    genny::Array* m_arr{};
    std::vector<std::unique_ptr<Variable>> m_elements{};
    std::vector<std::unique_ptr<genny::Variable>> m_proxy_variables{};
};
} // namespace node