#pragma once

#include <chrono>

#include "../Process.hpp"
#include "Variable.hpp"

namespace node {
class Pointer : public Variable {
public:
    Pointer(Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto is_collapsed(bool is_collapsed) {
        m_props["__collapsed"].value = is_collapsed;
        return this;
    }
    auto& is_collapsed() { return std::get<bool>(m_props["__collapsed"].value); }

    auto is_array(bool is_array) {
        m_props["__array"].value = is_array;
        return this;
    }
    auto& is_array() { return std::get<bool>(m_props["__array"].value); }

    auto array_count(int count) {
        m_props["__count"].value = count;
        return this;
    }
    auto& array_count() { return std::get<int>(m_props["__count"].value); }

protected:
    genny::Pointer* m_ptr{};
    std::vector<std::byte> m_mem{};
    std::chrono::steady_clock::time_point m_mem_refresh_time{};
    uintptr_t m_address{};

    std::unique_ptr<Base> m_ptr_node{};
    std::unique_ptr<genny::Variable> m_proxy_var{};

    void refresh_memory();
};
} // namespace node