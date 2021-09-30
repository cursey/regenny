#pragma once

#include <map>
#include <future>
#include <functional>

#include "Variable.hpp"

namespace node {
class Struct : public Variable {
public:
    Struct(Process& process, genny::Variable* var, Property& props);
    virtual ~Struct();

    void display(uintptr_t address, uintptr_t offset, std::byte* mem) override;
    void update(uintptr_t address, uintptr_t offset, std::byte* mem) override;

    auto is_collapsed(bool is_collapsed) {
        m_props["__collapsed"].set(is_collapsed);
        return this;
    }
    auto& is_collapsed() { return m_props["__collapsed"].as_bool(); }

    auto display_self(bool display_self) {
        m_display_self = display_self;
        return this;
    }
    auto display_self() const { return m_display_self; }

private:
    bool m_display_self{true};
    genny::Struct* m_struct{};
    std::multimap<uintptr_t, std::shared_ptr<Base>> m_nodes{};
    bool m_is_hovered{};
    std::string m_display_str{};

    std::vector<std::byte> m_task_data{};
    std::future<bool> m_update_result{};
    std::unique_ptr<std::thread> m_update_thread{};

    void fill_space(uintptr_t last_offset, int delta);
};

} // namespace node