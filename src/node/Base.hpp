#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "../Config.hpp"
#include "../Process.hpp"
#include "Property.hpp"

namespace node {
class Base {
public:
    Base(Config& cfg, Process& process, Property& props);

    virtual void display(uintptr_t address, uintptr_t offset, std::byte* mem) = 0;
    virtual size_t size() = 0;
    virtual void update(uintptr_t address, uintptr_t offset, std::byte* mem);

    auto& props() { return m_props; }

protected:
    static int indentation_level;
    Config& m_cfg;
    Process& m_process;
    Property& m_props;
    std::string m_preamble_str{};
    std::string m_bytes_str{};
    std::string m_print_str{};

    void display_address_offset(uintptr_t address, uintptr_t offset);
};

} // namespace node
