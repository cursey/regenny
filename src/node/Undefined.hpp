#pragma once

#include "Base.hpp"

namespace node {
class Undefined : public Base {
public:
    Undefined(Process& process, Property& props, size_t size, Undefined* parent = nullptr);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;
    void on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto is_split(bool split) {
        m_props["__split"].value = split;
        return this;
    }
    auto& is_split() { return std::get<bool>(m_props["__split"].value); }

    auto size_override(int size) { 
        m_props["__size"].value = size;
        return this;
    }
    auto& size_override() { return std::get<int>(m_props["__size"].value); }

protected:
    size_t m_size{};
    size_t m_original_size{};
    std::string m_bytes_str{};
    std::string m_preview_str{};

    Undefined* m_parent{};
    std::unique_ptr<Undefined> m_split0{};
    std::unique_ptr<Undefined> m_split1{};
};
} // namespace node