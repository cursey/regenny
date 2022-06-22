#pragma once

#include <chrono>

#include "../Process.hpp"
#include "Variable.hpp"

namespace node {
class Pointer : public Variable {
public:
    Pointer(Config& cfg, Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto is_collapsed(bool is_collapsed) {
        m_props["__collapsed"].set(is_collapsed);
        return this;
    }
    auto& is_collapsed() { return m_props["__collapsed"].as_bool(); }

    auto is_array(bool is_array) {
        m_props["__array"].set(is_array);
        return this;
    }
    auto& is_array() { return m_props["__array"].as_bool(); }

    auto array_count(int count) {
        m_props["__count"].set(count);
        return this;
    }
    auto& array_count() { return m_props["__count"].as_int(); }

protected:
    genny::Pointer* m_ptr{};
    std::vector<std::byte> m_mem{};
    std::chrono::steady_clock::time_point m_mem_refresh_time{};
    uintptr_t m_address{};

    std::unique_ptr<Base> m_ptr_node{};
    std::unique_ptr<genny::Variable> m_proxy_var{};

    std::string m_value_str{};
    std::string m_address_str{};

    bool m_is_hovered{};

    void refresh_memory();

    static void display_str(std::string& s, const std::string& str);
};
} // namespace node