#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <type_traits>

#include <LuaGenny.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <nfd.h>
#include <sdkgenny_parser.hpp>
#include <spdlog/spdlog.h>

#include "AboutUi.hpp"
#include "Utility.hpp"
#include "arch/Arch.hpp"
#include "node/Undefined.hpp"

#ifdef _WIN32
#include "arch/Windows.hpp"
#endif

#include "ReGenny.hpp"

using namespace std::literals;

ReGenny::ReGenny(SDL_Window* window)
    : m_window{window}, m_helpers{arch::make_helpers()}, m_process{std::make_unique<Process>()} {
    spdlog::set_default_logger(m_logger.logger());
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::info("Start of log.");

    reset_lua_state();

    auto path_str = SDL_GetPrefPath("cursey", "ReGenny");
    m_app_path = path_str;
    SDL_free(path_str);

    load_cfg();

    m_triggers.on({SDLK_LCTRL, SDLK_n}, [this] { file_new(); });
    m_triggers.on({SDLK_LCTRL, SDLK_o}, [this] { file_open(); });
    m_triggers.on({SDLK_LCTRL, SDLK_s}, [this] { file_save(); });
    m_triggers.on({SDLK_LCTRL, SDLK_q}, [this] { file_quit(); });
    m_triggers.on({SDLK_LCTRL, SDLK_l}, [this] { file_run_lua_script(); });
    m_triggers.on({SDLK_LCTRL, SDLK_e}, [this] { file_open_in_editor(); });
}

ReGenny::~ReGenny() {
}

void ReGenny::process_event(SDL_Event& e) {
    m_triggers.processEvent(e);
}

void ReGenny::update() {
    if (m_load_font) {
        spdlog::info("Setting font {}...", m_cfg.font_file);

        auto& io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF(m_cfg.font_file.c_str(), m_cfg.font_size);
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        m_load_font = false;
    }

    auto now = std::chrono::system_clock::now();

    // Auto reload changed files.
    if (now > m_file_modified_check_time) {
        file_reload();
        m_file_modified_check_time = now + 1s;
    }

    // Auto detach from closed processes.
    if (m_process != nullptr) {
        if (!m_process->ok()) {
            action_detach();
        }
    }

    if (m_cfg_save_time && now > *m_cfg_save_time) {
        save_cfg();
        m_cfg_save_time = std::nullopt;
    }
}

void ReGenny::ui() {
    SDL_GetWindowSize(m_window, &m_window_w, &m_window_h);
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)m_window_w, (float)m_window_h}, ImGuiCond_Always);
    ImGui::Begin("ReGenny", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize);
    auto dock = ImGui::GetID("MainDockSpace");

    if (ImGui::DockBuilderGetNode(dock) == nullptr) {
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImGui::GetContentRegionAvail());

        ImGuiID left{}, right{};
        ImGuiID top{}, bottom{};

        ImGuiID bottom_top{}, bottom_bottom{};

        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Up, 1.61f * 0.5f, &top, &bottom);
        ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.66f, &left, &right);
        ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Up, 1.61f * 0.5f, &bottom_top, &bottom_bottom);

        ImGui::DockBuilderDockWindow("Attach", left);
        ImGui::DockBuilderDockWindow("Memory View", left);
        ImGui::DockBuilderDockWindow("Editor", right);
        ImGui::DockBuilderDockWindow("Log", bottom_top);
        ImGui::DockBuilderDockWindow("LuaEval", bottom_bottom);

        ImGui::DockBuilderFinish(dock);
    }

    ImGui::DockSpace(dock, ImGui::GetContentRegionAvail(), ImGuiDockNodeFlags_AutoHideTabBar);

    menu_ui();

    ImGui::SetNextWindowPos(ImVec2{m_window_w / 2.0f, m_window_h / 2.0f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{600.0f, 0.0f}, ImGuiCond_Appearing);
    m_ui.attach_popup = ImGui::GetID("Attach");
    if (ImGui::BeginPopupModal("Attach")) {
        attach_ui();

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    m_ui.rtti_popup = ImGui::GetID("RTTI");

    if (ImGui::BeginPopupModal("RTTI")) {
        rtti_ui();

        ImGui::SameLine();

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    m_ui.rtti_sweep_popup = ImGui::GetID("RTTI Sweep");
    if (ImGui::BeginPopupModal("RTTI Sweep")) {
        rtti_sweep_ui();

        ImGui::SameLine();

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Begin("Memory View");
    memory_ui();
    ImGui::End();

    ImGui::Begin("Log");
    m_logger.ui();
    ImGui::End();

    ImGui::Begin("LuaEval");

    ImGui::BeginChild("luaeval");
    std::array<char, 256> eval{};
    ImGui::PushItemWidth(ImGui::GetWindowWidth());

    if (m_reapply_focus_eval) {
        ImGui::SetKeyboardFocusHere();
        m_reapply_focus_eval = false;
    }

    auto eval_cb = [](ImGuiInputTextCallbackData* data) -> int {
        auto rg = reinterpret_cast<ReGenny*>(data->UserData);

        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (rg->eval_history_index() > 0) {
                    rg->eval_history_index()--;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (rg->eval_history_index() < rg->eval_history().size()) {
                    rg->eval_history_index()++;
                }
            }

            if (rg->eval_history_index() < rg->eval_history().size()) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, rg->eval_history()[rg->eval_history_index()].c_str());
            }
        }

        return 0;
    };

    if (ImGui::InputText("eval", eval.data(), 256,
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, eval_cb, this)) {
        if (m_eval_history.size() >= 20) {
            m_eval_history.pop_front();
        }

        m_eval_history.push_back(eval.data());
        m_eval_history_index = m_eval_history.size();

        try {
            if (std::string_view{eval.data()} == "clear") {
                m_logger.clear();
            } else {
                auto result = m_lua->safe_script(std::string{"return "} + eval.data());

                if (!result.valid()) {
                    result = m_lua->safe_script(eval.data());

                    if (!result.valid()) {
                        sol::script_default_on_error(*m_lua, std::move(result));
                    }
                } else {
                    auto obj = result.get<sol::object>();

                    obj.push();
                    auto str = luaL_tolstring(*m_lua, -1, nullptr);

                    if (str != nullptr) {
                        spdlog::info("{}", str);
                    }

                    obj.pop();
                }
            }
        } catch (const std::exception& e) {
            if (std::string_view{e.what()}.find("<eof>") != std::string_view::npos) {
                // Try again without the return
                try {
                    auto result = m_lua->safe_script(eval.data());

                    if (!result.valid()) {
                        sol::script_default_on_error(*m_lua, std::move(result));
                    }
                } catch (const std::exception& e) {
                    spdlog::error("{}", e.what());
                } catch (...) {
                    spdlog::error("Unknown exception");
                }
            } else {
                spdlog::error("{}", e.what());
            }
        } catch (...) {
            spdlog::error("Unknown exception");
        }

        m_reapply_focus_eval = true;
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2{m_window_w / 2.0f, m_window_h / 2.0f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    m_ui.error_popup = ImGui::GetID("Error");
    if (ImGui::BeginPopupModal("Error")) {
        ImGui::TextUnformatted(m_ui.error_msg.c_str());

        if (ImGui::Button("Ok")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImVec2{m_window_w / 2.0f, m_window_h / 2.0f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{600.0f, 0.0f}, ImGuiCond_Appearing);
    m_ui.font_popup = ImGui::GetID("Set Font");
    if (ImGui::BeginPopupModal("Set Font")) {
        if (ImGui::Button("Browse")) {
            nfdchar_t* out_path{};

            if (NFD_OpenDialog("ttf", nullptr, &out_path) == NFD_OKAY) {
                m_cfg.font_file = out_path;
            }
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(m_cfg.font_file.c_str());
        ImGui::SliderFloat("Size", &m_cfg.font_size, 6.0f, 32.0f, "%.0f");

        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();

            if (!m_cfg.font_file.empty()) {
                m_load_font = true;
            }

            save_cfg();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImVec2{m_window_w / 2.0f, m_window_h / 2.0f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{600.0f, 350.0f}, ImGuiCond_Appearing);
    m_ui.about_popup = ImGui::GetID("About");
    if (ImGui::BeginPopup("About")) {
        about_ui();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImVec2{m_window_w / 2.0f, m_window_h / 2.0f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{600.0f, 0.0f}, ImGuiCond_Appearing);
    m_ui.extensions_popup = ImGui::GetID("Set SDK Extensions");
    if (ImGui::BeginPopupModal("Set SDK Extensions")) {
        ImGui::InputText("Header Extension", &m_project.extension_header);
        ImGui::InputText("Source Extension", &m_project.extension_source);

        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
            save_project();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::End();
}

void ReGenny::menu_ui() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                file_new();
            }

            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                file_open();
            }

            if (ImGui::BeginMenu("Open Recent...")) {
                if (m_cfg.file_history.empty()) {
                    ImGui::TextUnformatted("No files have been open recently");
                }

                for (auto&& path : m_cfg.file_history) {
                    if (ImGui::MenuItem(path.c_str())) {
                        file_open(path);
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::BeginDisabled(m_sdk == nullptr);

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                file_save();
            }

            if (ImGui::MenuItem("Save As...")) {
                file_save_as();
            }

            if (ImGui::MenuItem("Open In Editor", "Ctrl+E")) {
                file_open_in_editor();
            }

            if (ImGui::MenuItem("Run Lua Script", "Ctrl+L")) {
                file_run_lua_script();
            }

            ImGui::EndDisabled();

            if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                file_quit();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Hide Undefined Nodes", &node::Undefined::is_hidden);

            if (ImGui::Checkbox("Display Address", &m_cfg.display_address)) {
                save_cfg();
            }

            if (ImGui::Checkbox("Display Offset", &m_cfg.display_offset)) {
                save_cfg();
            }

            if (ImGui::Checkbox("Display Bytes", &m_cfg.display_bytes)) {
                save_cfg();
            }

            if (ImGui::Checkbox("Display Print", &m_cfg.display_print)) {
                save_cfg();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Action")) {
            ImGui::BeginDisabled(m_sdk == nullptr);

            if (ImGui::MenuItem("Attach")) {
                ImGui::OpenPopup(m_ui.attach_popup);
            }

            if (ImGui::MenuItem("Detach")) {
                action_detach();
            }

            if (ImGui::MenuItem("Generate SDK")) {
                action_generate_sdk();
            }

            if (ImGui::MenuItem("Autogenerate RTTI Struct")) {
                m_ui.rtti_text.clear();
                ImGui::OpenPopup(m_ui.rtti_popup);
            }

            if (ImGui::MenuItem("RTTI Sweep Scan")) {
                ImGui::OpenPopup(m_ui.rtti_sweep_popup);
            }

            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("Set Font")) {
                // options_set_font();
                ImGui::OpenPopup(m_ui.font_popup);
            }

            if (ImGui::MenuItem("Set SDK Extensions")) {
                ImGui::OpenPopup(m_ui.extensions_popup);
            }

            if (ImGui::SliderInt("Refresh delay", &m_cfg.refresh_rate, 0, 1000)) {
                m_cfg_save_time = std::chrono::system_clock::now() + 1s;
            }

            if (ImGui::Checkbox("Always on top", &m_cfg.always_on_top)) {
                save_cfg();
                SDL_SetWindowAlwaysOnTop(m_window, m_cfg.always_on_top ? SDL_TRUE : SDL_FALSE);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                ImGui::OpenPopup(m_ui.about_popup);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void ReGenny::file_reload() {
    // Check if a file was modified
    if (m_open_filepath.empty() || m_sdk == nullptr) {
        return;
    }

    for (auto&& filepath : m_sdk->imports()) {
        auto cwt = std::filesystem::last_write_time(filepath);

        try {
            if (cwt > m_file_lwt) {
                spdlog::info("Reopening {}...", filepath.string());
                parse_file();
                m_file_lwt = cwt;
                return;
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to get last write time for {}: {}", filepath.string(), e.what());
        } catch (...) {
            spdlog::error("Failed to get last write time for {} (Unknown reason)", filepath.string());
        }
    }
}

void ReGenny::file_new() {
    if (m_process && m_process->process_id() != 0) {
        action_detach();
    }

    nfdchar_t* out_path{};

    if (NFD_SaveDialog("genny", nullptr, &out_path) != NFD_OKAY) {
        return;
    }

    m_open_filepath = out_path;
    m_open_filepath.replace_extension("genny");

    free(out_path);

    spdlog::info("Creating new project {}...", m_open_filepath.string());

    std::ofstream genny_file{m_open_filepath};

    genny_file << "type int 4 [[i32]]\n\n"
               << "struct Foo {\n"
               << "    int bar @ 8\n"
               << "}\n";
    genny_file.close();

    save_project();
    parse_file();
}

void ReGenny::file_open(const std::filesystem::path& filepath) {
    if (m_process && m_process->process_id() != 0) {
        action_detach();
    }

    if (filepath.empty()) {
        nfdchar_t* out_path{};

        if (NFD_OpenDialog("genny", nullptr, &out_path) != NFD_OKAY) {
            return;
        }

        m_open_filepath = out_path;
        free(out_path);
    } else {
        m_open_filepath = filepath;
    }

    spdlog::info("Opening {}...", m_open_filepath.string());

    load_project();
    parse_file();
    remember_file();
    set_window_title();
}

void ReGenny::load_project() {
    auto project_filepath = m_open_filepath;
    project_filepath.replace_extension("json");

    if (!std::filesystem::exists(project_filepath)) {
        spdlog::warn("No project {} exists. Skipping...", project_filepath.string());
        return;
    }

    spdlog::info("Opening project {}...", project_filepath.string());

    try {
        std::ifstream f{project_filepath};
        nlohmann::json j{};

        f >> j;
        m_project = j.get<Project>();
    } catch (const nlohmann::json::exception& e) {
        spdlog::error(e.what());
    }

    if (m_process->process_id() == 0) { // Invalid PID (aka not attached).
        attach();
    }

    // Reset the memory UI here since a new project has been loaded.
    m_mem_ui.reset();
}

void ReGenny::file_save() {
    if (m_open_filepath.empty()) {
        file_save_as();
        return;
    }

    spdlog::info("Saving {}...", m_open_filepath.string());

    remember_file();
    save_project();
}

void ReGenny::save_project() {
    auto proj_filepath = m_open_filepath;

    proj_filepath.replace_extension("json");
    spdlog::info("Saving {}...", proj_filepath.string());

    try {
        std::ofstream f{proj_filepath, std::ofstream::out};
        nlohmann::json j = m_project;

        f << std::setw(4) << j;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error(e.what());
    }
}

void ReGenny::file_save_as() {
    nfdchar_t* save_path{};

    if (NFD_SaveDialog("genny", m_open_filepath.string().c_str(), &save_path) != NFD_OKAY) {
        return;
    }

    m_open_filepath = save_path;
    m_open_filepath.replace_extension("genny");

    spdlog::info("Saving as {}...", m_open_filepath.string());

    file_save();
    free(save_path);
}

void ReGenny::file_open_in_editor() {
    if (m_open_filepath.empty()) {
        return;
    }

    spdlog::info("Opening {}...", m_open_filepath.string());

    SDL_OpenURL(("file://" + m_open_filepath.string()).c_str());
}

void ReGenny::file_quit() {
    SDL_QuitEvent event{};

    event.timestamp = SDL_GetTicks();
    event.type = SDL_QUIT;

    SDL_PushEvent((SDL_Event*)&event);
}

void ReGenny::file_run_lua_script() {
    std::scoped_lock _{m_lua_lock};

    nfdchar_t* lua_path{};

    if (NFD_OpenDialog("lua", nullptr, &lua_path) != NFD_OKAY) {
        return;
    }

    try {
        m_lua->do_file(lua_path);
    } catch (const std::exception& e) {
        spdlog::error(e.what());
        return;
    } catch (...) {
        spdlog::error("Unknown error!");
        return;
    }
}

void ReGenny::action_detach() {
    spdlog::info("Detaching...");
    m_process = std::make_unique<Process>();
    m_mem_ui = std::make_unique<MemoryUi>(
        m_cfg, *m_sdk, dynamic_cast<sdkgenny::Struct*>(m_type), *m_process, m_project.props[m_project.type_chosen]);
    m_ui.processes.clear();
    set_window_title();
}

void ReGenny::action_generate_sdk() {
    if (m_sdk == nullptr) {
        return;
    }

    nfdchar_t* sdk_path{};

    if (NFD_PickFolder(nullptr, &sdk_path) != NFD_OKAY) {
        return;
    }

    spdlog::info("Generating SDK at {}...", sdk_path);
    m_sdk->header_extension(m_project.extension_header)
        ->source_extension(m_project.extension_source)
        ->generate(sdk_path);
    free(sdk_path);
    spdlog::info("SDK generated!");
}

void ReGenny::attach_ui() {
    auto now = std::chrono::steady_clock::now();

    if (m_ui.processes.empty() || now >= m_ui.next_attach_refresh_time) {
        m_ui.processes = m_helpers->processes();
        m_ui.next_attach_refresh_time = now + 1s;
    }

    ImGui::InputText("Filter", &m_project.process_filter);

    if (ImGui::BeginListBox("Processes")) {
        for (auto&& [pid, name] : m_ui.processes) {
            if (!m_project.process_filter.empty()) {
                if (auto it = std::search(name.begin(), name.end(), m_project.process_filter.begin(),
                        m_project.process_filter.end(), [](auto a, auto b) { return tolower(a) == tolower(b); });
                    it == name.end()) {
                    continue;
                }
            }
            auto is_selected = pid == m_project.process_id;

            if (ImGui::Selectable(fmt::format("{} - {}", pid, name).c_str(), is_selected)) {
                m_project.process_name = name;
                m_project.process_id = pid;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndListBox();
    }

    if (ImGui::Button("Attach")) {
        ImGui::CloseCurrentPopup();
        attach();
    }
}

void ReGenny::attach() {
    if (m_project.process_id == 0) {
        return;
    }

    // Make sure we have an up-to-date process list.
    m_ui.processes = m_helpers->processes();

    // Validate that the chosen process_id and process_name match.
    auto is_valid = false;

    for (auto&& [pid, name] : m_ui.processes) {
        if (pid == m_project.process_id && name == m_project.process_name) {
            is_valid = true;
        }
    }

    if (!is_valid) {
        return;
    }

    spdlog::info("Attaching to {} PID: {}...", m_project.process_name, m_project.process_id);

    m_process = arch::open_process(m_project.process_id);

    if (!m_process->ok()) {
        action_detach();
        m_ui.error_msg = "Couldn't open the process!";
        ImGui::OpenPopup(m_ui.error_popup);
        return;
    }

    parse_file();
    set_window_title();
}

void ReGenny::rtti_sweep_ui() {
    if (m_process == nullptr || !m_process->ok() || m_process->process_id() == 0) {
        ImGui::Text("Error: No Process");
        return;
    }

    if (!m_is_address_valid) {
        ImGui::Text("Error: Invalid Address");
        return;
    }

    if (m_type == nullptr) {
        ImGui::Text("Error: No Type");
        return;
    }

    if (m_type->size() == 0) {
        ImGui::Text("Error: Empty Type");
        return;
    }

    auto size = ImGui::GetContentRegionAvail();

    size.y -= 16;
    size.y = std::clamp(size.y, 0.0f, 128.0f);

    if (ImGui::InputTextMultiline("##sweep_rtti", &m_ui.rtti_sweep_text, size, ImGuiInputTextFlags_AllowTabInput)) {
    }

    if (ImGui::InputText("Class Name", &m_ui.rtti_sweep_search_name, ImGuiInputTextFlags_AllowTabInput)) {
    }

    if (ImGui::Button("Search")) {
        m_ui.rtti_sweep_text.clear();

        std::vector<uint8_t> base_data(m_type->size());
        m_process->read(m_address, base_data.data(), base_data.size());

        for (size_t i = 0; i < base_data.size(); i += sizeof(void*)) {
            if (i + sizeof(void*) >= base_data.size()) {
                break;
            }

            const auto deref = *(uintptr_t*)(base_data.data() + i);

            if (deref == 0) {
                continue;
            }

            const auto tname = m_process->get_typename(deref);

            if (!tname || tname->length() < 5) {
                continue;
            }

            if (tname && tname->find(m_ui.rtti_sweep_search_name) != std::string::npos) {
                m_ui.rtti_sweep_text += fmt::format("struct {:s}* {:s} @ 0x{:x}\n", *tname, "rtti", (uintptr_t)i);
            }
        }

        struct Chain {
            uintptr_t base;
            size_t offset;
        };

        static std::function<void(uintptr_t base, size_t size, std::vector<Chain>& chain, std::string_view class_name)> lookup{};
        lookup = [this](uintptr_t base, size_t size, std::vector<Chain>& chain, std::string_view class_name) {
            if (chain.size() > 2) {
                return;
            }

            std::vector<uint8_t> data(size);
            if (!m_process->read(base, data.data(), size)) {
                return;
            }

            for (size_t i = 0; i < data.size(); i += sizeof(void*)) {
                if (i + sizeof(void*) >= data.size()) {
                    break;
                }

                const auto deref = *(uintptr_t*)(data.data() + i);

                if (deref == 0) {
                    continue;
                }

                const auto tname = m_process->get_typename(deref);

                if (tname && tname->find(class_name) != std::string::npos) {
                    std::string chain_string{};
                    for (auto&& c : chain) {
                        chain_string += fmt::format("0x{:x} -> ", c.offset);
                    }
                    m_ui.rtti_sweep_text += fmt::format("struct {:s}* @ {:s} + 0x{:x}\n", *tname, chain_string, i);
                }
                
                chain.push_back({base, i});
                lookup(deref, 0x1000, chain, class_name);
                chain.pop_back();
            }
        };

        std::vector<Chain> chain{};
        lookup(m_address, m_type->size(), chain, m_ui.rtti_sweep_search_name);
    }
}

void ReGenny::rtti_ui() {
    if (m_process == nullptr || !m_process->ok() || m_process->process_id() == 0) {
        ImGui::Text("Error: No Process");
        return;
    }

    if (!m_is_address_valid) {
        ImGui::Text("Error: Invalid Address");
        return;
    }

    if (m_type == nullptr) {
        ImGui::Text("Error: No Type");
        return;
    }

    if (m_type->size() == 0) {
        ImGui::Text("Error: Empty Type");
        return;
    }

    auto size = ImGui::GetContentRegionAvail();

    size.y -= 16;
    size.y = std::clamp(size.y, 0.0f, 128.0f);

    if (ImGui::InputTextMultiline("##source_rtti", &m_ui.rtti_text, size, ImGuiInputTextFlags_AllowTabInput)) {
    }

    if (ImGui::Button("Generate")) {
        m_ui.rtti_text.clear();
        std::vector<uint8_t> data(m_type->size());

        m_process->read(m_address, data.data(), data.size());

        std::unordered_map<std::string, uint32_t> counts{};

        for (size_t i = 0; i < data.size(); i++) {
            if (i + sizeof(void*) >= data.size()) {
                break;
            }

            const auto deref = *(uintptr_t*)(data.data() + i);

            if (deref == 0) {
                continue;
            }

            const auto tname = m_process->get_typename(deref);

            if (!tname || tname->length() < 5) {
                continue;
            }

            std::vector<uint8_t> bad_chars{
                '<', '>', ':', '"', '/', '\\', '|', '?', '*', '\0', '\a', '\b', '\f', '\n', '\r', '\t',
                ' ', ',', ';', '=', '(', ')', '[', ']', '{', '}'
            };

            std::string demangled{};
            demangled.reserve(tname->length());
            
            // replace bad characters with underscores
            for (auto&& c : *tname) {
                if (std::find(bad_chars.begin(), bad_chars.end(), c) != bad_chars.end()) {
                    demangled += '_';
                } else {
                    demangled += c;
                }
            }

            if (demangled.starts_with("class")) {
                demangled = demangled.substr(5);
            } else if (demangled.starts_with("struct")) {
                demangled = demangled.substr(6);
            }

            counts[demangled] += 1;
            const auto count = counts[demangled];

            const auto variable_name = demangled + "_" + std::to_string(count);

            m_ui.rtti_text += fmt::format("struct {:s}* {:s} @ 0x{:x}\n", demangled, variable_name, (uintptr_t)i);
        }
    }
}

void ReGenny::update_address() {
    // If the parsed address has no offsets then it's not valid at all and there's nothing to update.
    if (m_parsed_address.offsets.empty()) {
        return;
    }

    // Make sure it's time to update.
    auto now = std::chrono::steady_clock::now();

    if (now < m_next_address_refresh_time) {
        return;
    }

    m_next_address_refresh_time = now + 250ms;
    m_is_address_valid = false;

    // Get the starting point.
    m_address = m_parsed_address.offsets.front();

    if (!m_parsed_address.name.empty()) {
        auto& modname = m_parsed_address.name;

        for (auto&& mod : m_process->modules()) {
            if (std::equal(modname.begin(), modname.end(), mod.name.begin(), mod.name.end(),
                    [](auto a, auto b) { return std::tolower(a) == std::tolower(b); })) {
                m_address += mod.start;
                break;
            }
        }
    }

    // Dereference and add the offsets.
    for (auto it = m_parsed_address.offsets.begin() + 1; it != m_parsed_address.offsets.end(); ++it) {
        m_address = m_process->read<uintptr_t>(m_address).value_or(0);

        if (m_address == 0) {
            return;
        }

        m_address += *it;
    }

    // Validate the final address.
    for (auto&& allocation : m_process->allocations()) {
        if (allocation.start <= m_address && m_address <= allocation.end) {
            m_is_address_valid = true;
            break;
        }
    }
}

void ReGenny::memory_ui() {
    // assert(m_process != nullptr);

    if (ImGui::BeginCombo("Typename", m_project.type_chosen.c_str())) {
        for (auto&& type_name : m_ui.type_names) {
            auto is_selected = type_name == m_project.type_chosen;

            if (ImGui::Selectable(type_name.c_str(), is_selected)) {
                // Save the previously selected type's props.
                if (m_mem_ui != nullptr) {
                    m_project.props[m_project.type_chosen] = m_mem_ui->props();
                }

                m_project.type_chosen = type_name;
                set_type();
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    if (ImGui::InputText("Address", &m_ui.address)) {
        set_address();
    }

    ImGui::SameLine();

    if (!m_is_address_valid) {
        ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Invalid address!");
        // return;
    } else {
        ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "%p", m_address);
    }

    update_address();

    if (m_mem_ui != nullptr) {
        m_mem_ui->display(m_is_address_valid ? m_address : 0);
    }
}

void ReGenny::set_address() {
    if (auto addr = parse_address(m_ui.address)) {
        m_parsed_address = *addr;
    }

    remember_type_and_address();
}

void ReGenny::set_type() {
    if (m_sdk == nullptr) {
        return;
    }

    sdkgenny::Object* parent = m_sdk->global_ns();
    std::string type_name = m_project.type_chosen;
    size_t pos{};

    while ((pos = type_name.find('.')) != std::string::npos) {
        parent = parent->find<sdkgenny::Object>(type_name.substr(0, pos));

        if (parent == nullptr) {
            return;
        }

        type_name.erase(0, pos + 1);
    }

    m_type = parent->find<sdkgenny::Struct>(type_name);

    if (m_type == nullptr) {
        return;
    }

    if (auto search = m_project.type_addresses.find(m_project.type_chosen); search != m_project.type_addresses.end()) {
        m_ui.address = search->second;
    } else {
        m_ui.address.clear();
    }

    set_address();

    m_mem_ui = std::make_unique<MemoryUi>(
        m_cfg, *m_sdk, dynamic_cast<sdkgenny::Struct*>(m_type), *m_process, m_project.props[m_project.type_chosen]);
}

void ReGenny::reset_lua_state() {
    std::scoped_lock _{m_lua_lock};

    m_lua = std::make_unique<sol::state>();
    auto& lua = *m_lua;

    m_lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::math, sol::lib::table,
        sol::lib::bit32, sol::lib::utf8, sol::lib::os, sol::lib::coroutine, sol::lib::io);
    luaopen_luagenny(lua);

    // clang-format off

    lua["sdkgenny"] = sol::stack::pop<sol::table>(*m_lua);
    lua["print"] = [](sol::this_state s, sol::object value) {
        if (value.is<const char*>()) {
            spdlog::info("{}", value.as<const char*>());
        } else {
            auto str = luaL_tolstring(s, lua_gettop(s), nullptr);

            spdlog::info("{}", str != nullptr ? str : "");

            lua_pop(s, 1);
        }
    };

    auto create_overlay = lua.safe_script("return function(addr, t) return sdkgenny.StructOverlay(addr, t) end").get<sol::function>();
    
    m_lua->new_usertype<ReGenny>("ReGennyClass",
        sol::no_constructor,
        "type", &ReGenny::type,
        "address", &ReGenny::address,
        "overlay", [create_overlay](sol::this_state s, ReGenny* rg) -> sol::object {
            if (rg->process() == nullptr) {
                return sol::make_object(s, sol::nil);
            }

            if (rg->type() == nullptr || !rg->type()->is_a<sdkgenny::Struct>()) {
                return sol::make_object(s, sol::nil);
            }

            return sol::make_object(s, create_overlay(rg->address(), dynamic_cast<sdkgenny::Struct*>(rg->type())));
        },
        "sdk", [](sol::this_state s, ReGenny* rg) { 
            if (rg->sdk() == nullptr) {
                sol::make_object(s, sol::nil);
            }

            return sol::make_object(s, rg->sdk().get());
        },
        "process", [](sol::this_state s, ReGenny* rg) -> sol::object { 
            if (rg->process() == nullptr) {
                sol::make_object(s, sol::nil);
            }

        #ifdef _WIN32
            if (auto wp = dynamic_cast<arch::WindowsProcess*>(rg->process().get())) {
                return sol::make_object(s, wp);
            }
        #endif
            
            return sol::make_object(s, rg->process().get());
        }
    );

    auto read_string = [](Process* p, uintptr_t addr, bool perform_strlen) -> std::string { 
        if (!perform_strlen) {
            std::vector<uint8_t> bytes(256);
            p->read(addr, bytes.data(), bytes.size());

            return std::string{(const char*)bytes.data()};
        }

        std::vector<uint8_t> bytes(16);

        for (size_t i = 0; ; i++) {
            const auto data = p->read<uint8_t>(addr + i);

            if (!data) {
                break;
            }

            if (i >= bytes.size()) {
                bytes.resize(bytes.size() * 2);
            }

            bytes[i] = *data;

            if (bytes[i] == 0) {
                break;
            }
        }

        if (bytes.empty()) {
            return std::string{};
        }

        return std::string{(const char*)bytes.data()};
    };

    m_lua->new_usertype<Process>("ReGennyProcess",
        sol::no_constructor,
        "read_uint8", [](Process* p, uintptr_t addr) { return p->read<uint8_t>(addr); },
        "read_uint16", [](Process* p, uintptr_t addr) { return p->read<uint16_t>(addr); },
        "read_uint32", [](Process* p, uintptr_t addr) { return p->read<uint32_t>(addr); },
        "read_uint64", [](Process* p, uintptr_t addr) { return p->read<uint64_t>(addr); },
        "read_int8", [](Process* p, uintptr_t addr) { return p->read<int8_t>(addr); },
        "read_int16", [](Process* p, uintptr_t addr) { return p->read<int16_t>(addr); },
        "read_int32", [](Process* p, uintptr_t addr) { return p->read<int32_t>(addr); },
        "read_int64", [](Process* p, uintptr_t addr) { return p->read<int64_t>(addr); },
        "read_float", [](Process* p, uintptr_t addr) { return p->read<float>(addr); },
        "read_double", [](Process* p, uintptr_t addr) { return p->read<double>(addr); },
        "write_uint8", [](Process* p, uintptr_t addr, uint8_t val) { p->write<uint8_t>(addr, val); },
        "write_uint16", [](Process* p, uintptr_t addr, uint16_t val) { p->write<uint16_t>(addr, val); },
        "write_uint32", [](Process* p, uintptr_t addr, uint32_t val) { p->write<uint32_t>(addr, val); },
        "write_uint64", [](Process* p, uintptr_t addr, uint64_t val) { p->write<uint64_t>(addr, val); },
        "write_int8", [](Process* p, uintptr_t addr, int8_t val) { p->write<int8_t>(addr, val); },
        "write_int16", [](Process* p, uintptr_t addr, int16_t val) { p->write<int16_t>(addr, val); },
        "write_int32", [](Process* p, uintptr_t addr, int32_t val) { p->write<int32_t>(addr, val); },
        "write_int64", [](Process* p, uintptr_t addr, int64_t val) { p->write<int64_t>(addr, val); },
        "write_float", [](Process* p, uintptr_t addr, float val) { p->write<float>(addr, val); },
        "write_double", [](Process* p, uintptr_t addr, double val) { p->write<double>(addr, val); },
        "read_string", read_string,
        "protect", &Process::protect,
        "allocate", [](Process* p, uintptr_t addr, size_t size, sol::object flags_obj) {
            uint64_t flags{};

            if (flags_obj.is<uint64_t>()) {
                flags = flags_obj.as<uint64_t>();
            }

            return p->allocate(addr, size, flags);
        },
        "get_module_within", &Process::get_module_within,
        "get_module", &Process::get_module,
        "modules", &Process::modules,
        "allocations", &Process::allocations
    );

#ifdef _WIN32
    m_lua->new_usertype<arch::WindowsProcess>("ReGennyWindowsProcess",
        sol::base_classes, sol::bases<Process>(),
        "get_typename", &arch::WindowsProcess::get_typename,
        "get_typename_from_vtable", &arch::WindowsProcess::get_typename_from_vtable,
        "allocate_rwx", [](arch::WindowsProcess* p, uintptr_t addr, size_t size) {
            return p->allocate(addr, size, PAGE_EXECUTE_READWRITE);
        },
        "protect_rwx", [](arch::WindowsProcess* p, uintptr_t addr, size_t size) {
            return p->protect(addr, size, PAGE_EXECUTE_READWRITE);
        },
        "create_remote_thread", &arch::WindowsProcess::create_remote_thread
    );
#endif

    m_lua->new_usertype<Process::Module>("ReGennyProcessModule",
        "name", &Process::Module::name,
        "start", &Process::Module::start,
        "end", &Process::Module::end,
        "size", &Process::Module::size
    );

    m_lua->new_usertype<Process::Allocation>("ReGennyProcessAllocation",
        "start", &Process::Allocation::start,
        "end", &Process::Allocation::end,
        "size", &Process::Allocation::size,
        "read", &Process::Allocation::read,
        "write", &Process::Allocation::write,
        "execute", &Process::Allocation::execute
    );

    lua["regenny"] = this;

    lua["sdkgenny_reader"] = [](sol::this_state s, uintptr_t address, size_t size) -> sol::object {
        // Make this use ReGenny's process reading functions
        // instead of treating the address as if we're in the same context as the game.

        auto lua = sol::state_view{s};
        auto rg = lua["regenny"].get<ReGenny*>();

        if (rg == nullptr) {
            return sol::make_object(s, sol::nil);
        }

        auto& process = rg->process();

        if (process == nullptr) {
            return sol::make_object(s, sol::nil);
        }

        switch (size) {
        case 8: {
            auto value = process->read<uint64_t>(address);
            if (!value) {
                break;
            }

            return sol::make_object(s, value);
        }
        case 4: {
            auto value = process->read<uint32_t>(address);
            if (!value) {
                break;
            }

            return sol::make_object(s, value);
        }
        case 2: {
            auto value = process->read<uint16_t>(address);
            if (!value) {
                break;
            }

            return sol::make_object(s, value);
        }
        case 1: {
            auto value = process->read<uint8_t>(address);
            if (!value) {
                break;
            }

            return sol::make_object(s, value);
        }
        default:
            break;
        }

        return sol::make_object(s, sol::nil);
    };

    lua["sdkgenny_string_reader"] = [read_string](sol::this_state s, uintptr_t address) -> sol::object {
        auto lua = sol::state_view{s};
        auto rg = lua["regenny"].get<ReGenny*>();

        if (rg == nullptr) {
            return sol::make_object(s, sol::nil);
        }

        auto& process = rg->process();

        if (process == nullptr) {
            return sol::make_object(s, sol::nil);
        }

        return sol::make_object(s, read_string(process.get(), address, true));
    };

    lua["sdkgenny_writer"] = [](sol::this_state s, uintptr_t address, size_t size, sol::object value) {
        auto lua = sol::state_view{s};
        auto rg = lua["regenny"].get<ReGenny*>();

        if (rg == nullptr) {
            return;
        }

        auto& process = rg->process();

        if (process == nullptr) {
            return;
        }

        switch(size) {
        case 8:
            if (!value.is<sol::nil_t>()) {
                value.push();

                if (lua_isinteger(s, -1)) {
                    process->write<uint64_t>(address, (uint64_t)lua_tointeger(s, -1));
                } else if (lua_isnumber(s, -1)) {
                    process->write<double>(address, lua_tonumber(s, -1));
                }

                value.pop();
            } else {
                process->write<uint64_t>(address, 0);
            }

            break;
        case 4:
            if (!value.is<sol::nil_t>()) {
                value.push();

                if (lua_isinteger(s, -1)) {
                    process->write<uint32_t>(address, (uint32_t)lua_tointeger(s, -1));
                } else if (lua_isnumber(s, -1)) {
                    process->write<float>(address, (float)lua_tonumber(s, -1));
                }

                value.pop();
            } else {
                process->write<uint32_t>(address, 0);
            }

            break;
        case 2:
            process->write<uint16_t>(address, value.as<uint16_t>());
            break;
        case 1:
            value.push();

            if (lua_isboolean(s, -1)) {
                process->write<bool>(address, lua_toboolean(s, -1));
            } else if (lua_isinteger(s, -1)) {
                process->write<uint8_t>(address, (uint8_t)lua_tointeger(s, -1));
            }

            value.pop();

            break;
        }
    };

    // clang-format on
}

void ReGenny::parse_file() try {
    auto sdk = std::make_unique<sdkgenny::Sdk>();

    sdk->import(m_open_filepath);

    sdkgenny::parser::State s{};
    s.filepath = m_open_filepath;
    s.parents.push_back(sdk->global_ns());

    tao::pegtl::file_input in{m_open_filepath};

    if (tao::pegtl::parse<sdkgenny::parser::Grammar, sdkgenny::parser::Action>(in, s)) {
        // We just parsed, so record the max last write time for any of the imported files.
        // This prevents reloading on opening a file for the first time since launch.
        for (auto&& import : sdk->imports()) {
            m_file_lwt = std::max(m_file_lwt, std::filesystem::last_write_time(import));
        }

        m_sdk = std::move(sdk);

        if (m_mem_ui != nullptr) {
            m_project.props[m_project.type_chosen] = m_mem_ui->props();
            m_mem_ui.reset();
        }

        // Build the list of selectable types for the type selector.
        m_ui.type_names.clear();

        std::unordered_set<sdkgenny::Struct*> structs{};
        m_sdk->global_ns()->get_all_in_children<sdkgenny::Struct>(structs);

        for (auto&& struct_ : structs) {
            std::vector<std::string> parent_names{};

            for (auto p = struct_->owner<sdkgenny::Object>(); p != nullptr && !p->is_a<sdkgenny::Sdk>();
                 p = p->owner<sdkgenny::Object>()) {
                if (auto& name = p->name(); !name.empty()) {
                    parent_names.emplace_back(name);
                }
            }

            std::reverse(parent_names.begin(), parent_names.end());
            std::string name{};

            for (auto p : parent_names) {
                name += p + '.';
            }

            name += struct_->name();

            m_ui.type_names.emplace(std::move(name));
        }

        set_type();
    } else {
        throw std::runtime_error{"Failed to parse file."}; 
    }
} catch (const std::exception& e) {
    spdlog::error(e.what());
}

void ReGenny::load_cfg() try {
    auto cfg_path = (m_app_path / "cfg.json").string();

    if (!std::filesystem::exists(cfg_path)) {
        return;
    }

    spdlog::info("Loading config {}...", cfg_path);

    std::ifstream f{cfg_path};
    nlohmann::json j{};

    f >> j;
    m_cfg = j.get<Config>();

    if (!m_cfg.font_file.empty()) {
        m_load_font = true;
    }

    SDL_SetWindowAlwaysOnTop(m_window, m_cfg.always_on_top ? SDL_TRUE : SDL_FALSE);
} catch (const std::exception& e) {
    spdlog::error(e.what());
    m_cfg = {};
}

void ReGenny::save_cfg() {
    auto cfg_path = m_app_path / "cfg.json";

    spdlog::info("Saving config {}...", cfg_path.string());

    try {
        std::ofstream f{cfg_path};
        nlohmann::json j = m_cfg;

        f << std::setw(4) << j;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error(e.what());
    }
}

void ReGenny::remember_file() {
    for (auto it = m_cfg.file_history.begin(); it != m_cfg.file_history.end();) {
        if (*it == m_open_filepath) {
            it = m_cfg.file_history.erase(it);
        } else {
            ++it;
        }
    }

    m_cfg.file_history.emplace_front(m_open_filepath.string());

    if (m_cfg.file_history.size() > 10) {
        m_cfg.file_history.resize(10);
    }

    save_cfg();
}

void ReGenny::remember_type_and_address() {
    if (m_type == nullptr) {
        return;
    }

    m_project.type_addresses[m_project.type_chosen] = m_ui.address;
}

void ReGenny::set_window_title() {
    std::string title = "ReGenny";

    if (!m_open_filepath.empty()) {
        title += fmt::format(" - {}", m_open_filepath.string());
    }

    if (m_process && m_process->process_id() != 0 && !m_project.process_name.empty()) {
        title += fmt::format(" - {} PID: {}", m_project.process_name, m_project.process_id);
    }

    SDL_SetWindowTitle(m_window, title.c_str());
}
