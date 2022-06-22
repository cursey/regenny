#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Bitfield : public Variable {
public:
    Bitfield(Config& cfg, Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

private:
    std::string m_display_str{};

    void write_display(uintptr_t address, std::byte* mem);
};
} // namespace node