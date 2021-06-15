#pragma once

#include "../Genny.hpp"
#include "Base.hpp"

namespace node {
class Variable : public Base {
public:
    Variable(Process& process, genny::Variable* var);

    virtual void display_type();
    virtual void display_name();
    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

protected:
    genny::Variable* m_var{};
    size_t m_size{};
    std::string m_value_str{};
};
} // namespace node