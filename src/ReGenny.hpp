#pragma once

#include <chrono>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <Genny.hpp>
#include <SDL.h>

#include "Config.hpp"
#include "Helpers.hpp"
#include "LoggerUi.hpp"
#include "MemoryUi.hpp"
#include "Process.hpp"
#include "Project.hpp"
#include "Utility.hpp"
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
    std::unique_ptr<genny::Sdk> m_sdk{};
    genny::Type* m_type{};
    uintptr_t m_address{};
    bool m_is_address_valid{};
    ParsedAddress m_parsed_address{};
    std::chrono::steady_clock::time_point m_next_address_refresh_time{};

    struct {
        // Process ID -> process name.
        std::map<uint32_t, std::string> processes{};
        std::chrono::steady_clock::time_point next_attach_refresh_time{};

        std::string error_msg{};
        ImGuiID error_popup{};

        std::string address{};
        std::set<std::string> type_names{};

        std::string editor_text{};
        std::string editor_error_msg{};

        std::string rtti_text{};

        ImGuiID attach_popup{};
        ImGuiID rtti_popup{};
        ImGuiID font_popup{};
        ImGuiID about_popup{};
        ImGuiID extensions_popup{};
    } m_ui{};

    std::unique_ptr<MemoryUi> m_mem_ui{};

    std::filesystem::path m_open_filepath{};
    std::filesystem::file_time_type m_file_lwt;

    LoggerUi m_logger{};
    bool m_log_parse_errors{};

    bool m_load_font{};

    bool m_reload_file{};

    std::filesystem::path m_app_path{};
    Trigger::Group m_triggers{};

    Config m_cfg{};
    Project m_project{};

    void menu_ui();

    void file_open(const std::filesystem::path& filepath = {});
    void file_reload();
    void load_project();
    void file_save();
    void save_project();
    void file_save_as();
    void file_quit();

    void action_detach();
    void action_generate_sdk();

    void attach_ui();
    void attach();

    void rtti_ui();

    void update_address();
    void memory_ui();
    void set_address();
    void set_type();

    void editor_ui();
    void parse_editor_text();

    void load_cfg();
    void save_cfg();

    void remember_file();
    void remember_type_and_address();
};
