#pragma once

#include <map>

#include "Variable.hpp"

namespace node {
class Bitfield : public Variable {
public:
    Bitfield(Process& process, genny::Bitfield* bf, Property& props);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

private:
    class Field : public Base {
    public:
        Field(Process& process, genny::Bitfield::Field* field);

        void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
        size_t size() override;
        void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

        auto&& field() const { return m_field; }
        auto&& field() { return m_field; }

    private:
        genny::Bitfield::Field* m_field{};
        std::string m_display_str{};
    };

    genny::Bitfield* m_bf{};
    std::map<uintptr_t, std::unique_ptr<Field>> m_fields{};
};
} // namespace node