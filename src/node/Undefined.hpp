#pragma once

#include "Base.hpp"

namespace node {
class Undefined : public Base {
public:
    static bool is_hidden;

    Undefined(Config& cfg, Process& process, Property& props, size_t size);

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    size_t size() override;
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto size_override(int size) {
        m_props["__size"].set(size);
        return this;
    }
    auto& size_override() { return m_props["__size"].as_int(); }

protected:
    size_t m_size{};
    size_t m_original_size{};
    std::string m_bytes_str{};
    std::string m_preview_str{};
    bool m_is_pointer{};
};
} // namespace node