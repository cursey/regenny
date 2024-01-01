#pragma once

#include <chrono>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <SDL.h>
#include <sdkgenny.hpp>
#include <sol/sol.hpp>

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
    auto& lua() const { return *m_lua; }
    auto& sdk() const { return m_sdk; }
    auto type() const { return m_type; }
    auto& process() const { return m_process; }
    auto address() const { return m_address; }

    auto add_address_resolver(std::function<uintptr_t(const std::string&)> resolver) {
        auto id = m_address_resolvers.size();
        m_address_resolvers[id] = std::move(resolver);
        return id;
    }
    auto remove_address_resolver(uint32_t id) { m_address_resolvers.erase(id); }
    auto query_address_resolvers(const std::string& name) {
        auto addresses = std::vector<uintptr_t>{};
        for (auto& resolver : m_address_resolvers | std::views::values) {
            if (auto address = resolver(name)) {
                addresses.push_back(address);
            }
        }

        return addresses;
    }

    auto& eval_history_index() { return m_eval_history_index; }
    auto& eval_history() const { return m_eval_history; }

private:
    int m_window_w{};
    int m_window_h{};
    SDL_Window* m_window{};

    std::unique_ptr<Helpers> m_helpers{};
    std::unique_ptr<Process> m_process{};
    std::unique_ptr<sdkgenny::Sdk> m_sdk{};
    sdkgenny::Type* m_type{};
    uintptr_t m_address{};
    bool m_is_address_valid{};
    ParsedAddress m_parsed_address{};
    std::map<uint32_t, std::function<uintptr_t(const std::string&)>> m_address_resolvers{};
    std::chrono::steady_clock::time_point m_next_address_refresh_time{};

    struct {
        // Process ID -> process name.
        std::map<uint32_t, std::string> processes{};
        std::chrono::steady_clock::time_point next_attach_refresh_time{};

        std::string error_msg{};
        ImGuiID error_popup{};

        std::string address{};
        std::set<std::string> type_names{};

        std::string rtti_text{};

        std::recursive_mutex rtti_lock{};
        std::string rtti_sweep_text{};
        std::string rtti_sweep_search_name{};

        ImGuiID attach_popup{};
        ImGuiID rtti_popup{};
        ImGuiID rtti_sweep_popup{};
        ImGuiID font_popup{};
        ImGuiID about_popup{};
        ImGuiID extensions_popup{};
    } m_ui{};

    std::unique_ptr<MemoryUi> m_mem_ui{};

    std::filesystem::path m_open_filepath{};
    std::filesystem::file_time_type m_file_lwt;
    std::chrono::system_clock::time_point m_file_modified_check_time{};

    LoggerUi m_logger{};

    bool m_load_font{};

    std::filesystem::path m_app_path{};
    Trigger::Group m_triggers{};

    Config m_cfg{};
    std::optional<std::chrono::system_clock::time_point> m_cfg_save_time{};

    std::recursive_mutex m_lua_lock{};
    std::unique_ptr<sol::state> m_lua{};
    std::deque<std::string> m_eval_history{};
    int32_t m_eval_history_index{};
    bool m_reapply_focus_eval{false};

    Project m_project{};

    void menu_ui();

    void file_new();
    void file_open(const std::filesystem::path& filepath = {});
    void file_reload();
    void load_project();
    void file_save();
    void save_project();
    void file_save_as();
    void file_open_in_editor();
    void file_quit();
    void file_run_lua_script();

    void action_detach();
    void action_generate_sdk();

    void attach_ui();
    void attach();

    void rtti_ui();
    void rtti_sweep_ui();

    void update_address();
    void memory_ui();
    void set_address();
    void set_type();

    void parse_file();
    void reset_lua_state();

    void load_cfg();
    void save_cfg();

    void remember_file();
    void remember_type_and_address();

    void set_window_title();
};
