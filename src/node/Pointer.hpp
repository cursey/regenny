#pragma once

#include <chrono>

#include "../Process.hpp"
#include "Variable.hpp"

namespace node {
class Pointer : public Variable {
public:
    Pointer(Process& process, genny::Variable* var);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto collapse(bool collapse) {
        m_is_collapsed = collapse;
        return this;
    }
    auto collapse() const { return m_is_collapsed; }

protected:
    bool m_is_collapsed{true};
    bool m_is_array{false};
    int m_count{1};
    genny::Pointer* m_ptr{};
    std::vector<std::byte> m_mem{};
    std::chrono::steady_clock::time_point m_mem_refresh_time{};
    uintptr_t m_address{};

    std::unique_ptr<Base> m_ptr_node{};
    std::unique_ptr<genny::Variable> m_proxy_var{};

    void refresh_memory();
};
} // namespace node