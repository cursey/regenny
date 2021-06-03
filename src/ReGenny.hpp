#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <SFML/Graphics/RenderWindow.hpp>

#include "Genny.hpp"
#include "Helpers.hpp"
#include "Process.hpp"

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
    std::unique_ptr<genny::Sdk> m_sdk{};
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

        std::string editor_text{};
        std::string editor_error_msg{};
    } m_ui{};

    void ui();

    void attach_ui();
    void attach();

    void memory_ui();
    void refresh_memory();
    void draw_variable(genny::Variable* var, const std::vector<std::byte>& mem);
    void set_type();

    void editor_ui();
    void parse_editor_text();
};
