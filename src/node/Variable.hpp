#pragma once

#include <Genny.hpp>

#include "Base.hpp"

namespace node {
class Variable : public Base {
public:
    Variable(Config& cfg, Process& process, genny::Variable* var, Property& props);

    virtual void display_type();
    virtual void display_name();
    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

protected:
    genny::Variable* m_var{};
    size_t m_size{};
    std::string m_value_str{};
    std::string m_utf8{};
    std::u16string m_utf16{};
    std::u32string m_utf32{};

    void write_display(uintptr_t address, std::byte* mem);
};
} // namespace node