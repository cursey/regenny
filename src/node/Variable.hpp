#pragma once

#include "../Genny.hpp"
#include "Base.hpp"
#include "Property.hpp"

namespace node {
class Variable : public Base {
public:
    Variable(Process& process, genny::Variable* var, Property& props);

    virtual void display_type();
    virtual void display_name();
    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto& props() { return m_props; }

protected:
    genny::Variable* m_var{};
    size_t m_size{};
    std::string m_value_str{};
    Property& m_props;
};
} // namespace node