#pragma once

#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <SDL.h>
#include <toml++/toml.h>

#include "Genny.hpp"
#include "Helpers.hpp"
#include "LoggerUi.hpp"
#include "MemoryUi.hpp"
#include "Process.hpp"
#include "node/Property.hpp"
#include "sdl_trigger.h"

class ReGenny {
public:
    ReGenny(SDL_Window* window);
    virtual ~ReGenny();

    void process_event(SDL_Event& e);
    void update();
    void ui();

    auto&& window() const { return m_window; }

private:
    int m_window_w{};
    int m_window_h{};
    SDL_Window* m_window{};

    std::unique_ptr<Helpers> m_helpers{};
    std::unique_ptr<Process> m_process{};
    std::vector<std::unique_ptr<Module>> m_modules{};
    std::unique_ptr<genny::Sdk> m_sdk{};
    genny::Type* m_type{};
    uintptr_t m_address{};

    struct {
        // Process ID -> process name.
        std::map<uint32_t, std::string> processes{};
        std::string process_name{};
        uint32_t process_id{};
        std::string error_msg{};
        ImGuiID error_popup{};

        std::string address{};
        std::string type_name{};
        std::set<std::string> type_names{};

        std::string editor_text{};
        std::string editor_error_msg{};
        bool editor_has_saved{};

        std::string font_to_load{};
        float font_size{16.0f};
        ImGuiID font_popup{};

        ImGuiID about_popup{};
    } m_ui{};

    std::unique_ptr<MemoryUi> m_mem_ui{};
    node::Property m_inherited_props{};

    std::filesystem::path m_open_filepath{};

    LoggerUi m_logger{};
    bool m_log_parse_errors{};

    bool m_load_font{};

    toml::table m_cfg{};
    std::filesystem::path m_app_path{};
    std::deque<std::filesystem::path> m_file_history{};
    Trigger::Group m_triggers{};

    void menu_ui();

    void file_open(const std::filesystem::path& filepath = {});
    void file_save();
    void file_save_as();
    void file_quit();

    void action_detach();
    void action_generate_sdk();

    void attach_ui();
    void attach();

    void memory_ui();
    void set_address();
    void set_type();

    void editor_ui();
    void parse_editor_text();

    void load_cfg();
    void save_cfg();

    void remember_file();
};
