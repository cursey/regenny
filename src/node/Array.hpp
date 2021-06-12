#pragma once

#include "Variable.hpp"

namespace node {
class Array : public Variable {
public:
    Array(Process& process, genny::Variable* var);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;

protected:
    genny::Array* m_arr{};
    int m_start_element{};
    int m_num_elements_displayed{};
    std::vector<std::unique_ptr<Variable>> m_elements{};
    std::vector<std::unique_ptr<genny::Variable>> m_proxy_variables{};
};
} // namespace node