#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Bitfield : public Variable {
public:
    Bitfield(Process& process, genny::Variable* var, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

private:
    std::string m_display_str{};
};
} // namespace node