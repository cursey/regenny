#pragma once

#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <chrono>

#include <SFML/Graphics/RenderWindow.hpp>

#include "Helpers.hpp"
#include "Process.hpp"
#include "Genny.hpp"

class ReGenny {
public:
    ReGenny();
    virtual ~ReGenny();

    void run();

private:
    sf::RenderWindow m_window{sf::VideoMode{1024, 768}, "ReGenny"};
    sf::Vector2u m_window_size{m_window.getSize()};

    std::unique_ptr<Helpers> m_helpers{};
    std::unique_ptr<Process> m_process{};
    std::vector<std::unique_ptr<Module>> m_modules{};
    genny::Sdk m_sdk{};
    genny::Type* m_type{};
    std::unordered_map<uintptr_t, genny::Variable*> m_var_map{};
    uintptr_t m_address{};
    std::unordered_map<uintptr_t, std::vector<std::byte>> m_mem{};
    std::chrono::steady_clock::time_point m_next_refresh_time{};

    struct {
        // Process name -> process ID.
        std::map<std::string, uint32_t> processes{};
        std::string process_name{};
        uint32_t process_id{};
        std::string error_msg{};

        std::string address{};
        std::string type_name{};
    } m_ui{};

    void ui();
    void attach_ui();
    void attach();
    void memory_ui();
    void refresh_memory();

    void draw_variable_value(genny::Variable* var, const std::vector<std::byte>& mem);
};

