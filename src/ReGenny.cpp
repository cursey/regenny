#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <type_traits>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <nfd.h>
#include <spdlog/spdlog.h>

#include "AboutUi.hpp"
#include "Utility.hpp"
#include "arch/Arch.hpp"

#include "ReGenny.hpp"

ReGenny::ReGenny(SDL_Window* window) : m_window{window} {
    spdlog::set_default_logger(m_logger.logger());
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::info("Start of log.");

    auto path_str = SDL_GetPrefPath("cursey", "ReGenny");
    m_app_path = path_str;
    SDL_free(path_str);

    load_cfg();

    m_triggers.on({SDLK_LCTRL, SDLK_o}, [this] { file_open(); });
    m_triggers.on({SDLK_LCTRL, SDLK_s}, [this] { file_save(); });
    m_triggers.on({SDLK_LCTRL, SDLK_q}, [this] { file_quit(); });

    m_helpers = arch::make_helpers();
}

ReGenny::~ReGenny() {
}

void ReGenny::process_event(SDL_Event& e) {
    m_triggers.processEvent(e);
}

void ReGenny::update() {
    if (m_load_font) {
        spdlog::info("Setting font {}...", m_ui.font_to_load);

        auto& io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF(m_ui.font_to_load.c_str(), m_ui.font_size);
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        m_load_font = false;
    }
}

void ReGenny::ui() {
    SDL_GetWindowSize(m_window, &m_window_w, &m_window_h);
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)m_window_w, (float)m_window_h}, ImGuiCond_Always);
    ImGui::Begin("ReGenny", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto dock = ImGui::GetID("MainDockSpace");

    if (ImGui::DockBuilderGetNode(dock) == nullptr) {
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImGui::GetContentRegionAvail());

        ImGuiID left{}, right{};
        ImGuiID top{}, bottom{};

        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Up, 1.61f * 0.5f, &top, &bottom);
        ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.66f, &left, &right);

        ImGui::DockBuilderDockWindow("Attach", left);
        ImGui::DockBuilderDockWindow("Memory View", left);
        ImGui::DockBuilderDockWindow("Editor", right);
        ImGui::DockBuilderDockWindow("Log", bottom);

        ImGui::DockBuilderFinish(dock);
    }

    ImGui::DockSpace(dock, ImGui::GetContentRegionAvail(), ImGuiDockNodeFlags_AutoHideTabBar);

    // Resizing gets kinda wonky because the ImGui window will try to resize more often than the actual window (atleast
    // on Windows 10). If we add the ImGuiWindowFlags_NoResize option to the window flags above, the window becomes even
    // harder to resize for some reason. So we fix that by letting the ImGui resize normally and then just setting the
    // actual window size to whatever the ImGui window size is.
    auto win_w = (int)ImGui::GetWindowWidth();
    auto win_h = (int)ImGui::GetWindowHeight();

    if (win_w != m_window_w || win_h != m_window_h) {
        SDL_SetWindowSize(m_window, win_w, win_h);
        SDL_GetWindowSize(m_window, &m_window_w, &m_window_h);
    }

    menu_ui();

    if (m_process == nullptr) {
        ImGui::Begin("Attach");
        attach_ui();
        ImGui::End();
    } else {
        ImGui::Begin("Memory View");
        memory_ui();
        ImGui::End();
        ImGui::Begin("Editor");
        editor_ui();
        ImGui::End();
    }

    ImGui::Begin("Log");
    m_logger.ui();
    ImGui::End();

    m_ui.error_popup = ImGui::GetID("Error");
    if (ImGui::BeginPopupModal("Error")) {
        ImGui::Text(m_ui.error_msg.c_str());

        if (ImGui::Button("Ok")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2{500.0f, 150.0f}, ImGuiCond_Appearing);
    m_ui.font_popup = ImGui::GetID("Set Font");
    if (ImGui::BeginPopupModal("Set Font")) {
        if (ImGui::Button("Browse")) {
            nfdchar_t* out_path{};

            if (NFD_OpenDialog("ttf", nullptr, &out_path) == NFD_OKAY) {
                m_ui.font_to_load = out_path;
            }
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(m_ui.font_to_load.c_str());
        ImGui::SliderFloat("Size", &m_ui.font_size, 6.0f, 32.0f, "%.0f");

        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();

            if (!m_ui.font_to_load.empty()) {
                m_load_font = true;
            }

            m_cfg["font"]["file"].ref<std::string>() = m_ui.font_to_load;
            m_cfg["font"]["size"].ref<int64_t>() = m_ui.font_size;
            save_cfg();
        }

        ImGui::EndPopup();
    }

    m_ui.about_popup = ImGui::GetID("About");
    if (ImGui::BeginPopup("About")) {
        about_ui();
        ImGui::EndPopup();
    }

    ImGui::End();
}

void ReGenny::menu_ui() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                file_open();
            }

            if (ImGui::BeginMenu("Open Recent...")) {
                if (m_file_history.empty()) {
                    ImGui::TextUnformatted("No files have been open recently");
                }

                for (auto&& path : m_file_history) {
                    if (ImGui::MenuItem(path.string().c_str())) {
                        file_open(path);
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                file_save();
            }

            if (ImGui::MenuItem("Save As...")) {
                file_save_as();
            }

            if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                file_quit();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Action")) {
            if (ImGui::MenuItem("Detach")) {
                action_detach();
            }

            if (ImGui::MenuItem("Generate SDK")) {
                action_generate_sdk();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("Set Font")) {
                // options_set_font();
                ImGui::OpenPopup(m_ui.font_popup);
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

void ReGenny::file_open(const std::filesystem::path& filepath) {
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

    std::ifstream f{m_open_filepath, std::ifstream::in | std::ifstream::binary | std::ifstream::ate};

    if (!f) {
        spdlog::error("Failed to open {}!", m_open_filepath.string());
        return;
    }

    m_ui.editor_text.resize(f.tellg());
    f.seekg(0, std::ifstream::beg);
    f.read(m_ui.editor_text.data(), m_ui.editor_text.size());

    load_project();

    m_log_parse_errors = true;
    parse_editor_text();
    m_log_parse_errors = false;

    remember_file();
}

void ReGenny::load_project() {
    auto project_filepath = m_open_filepath;
    project_filepath.replace_extension("json");

    if (!std::filesystem::exists(project_filepath)) {
        spdlog::warn("No project {} exists. Skipping...", project_filepath.string());
        return;
    }

    spdlog::info("Opening project {}...", project_filepath.string());

    std::ifstream f{project_filepath};

    f >> m_project;

    m_ui.type_name = m_project.value("chosen_type", "");
    m_ui.process_filter = m_project.value("process_filter", "");
    m_ui.process_id = m_project.value<uint32_t>("process_id", 0);
    m_ui.process_name = m_project.value("process_name", "");
    m_props.clear();

    std::function<void(nlohmann::json&, node::Property&)> visit = [&visit](nlohmann::json& j, node::Property& prop) {
        if (j.empty()) {
            return;
        }

        for (auto it = j.begin(); it != j.end(); ++it) {
            auto& val = it.value();

            if (val.is_boolean()) {
                prop[it.key()].value = (bool)val;
            } else if (val.is_number()) {
                prop[it.key()].value = (int)val;
            } else if (val.is_object()) {
                visit(val, prop.props[it.key()]);
            }
        }
    };

    try {
        for (auto it = m_project["props"].begin(); it != m_project["props"].end(); ++it) {
            visit(it.value(), m_props[it.key()]);
        }
    } catch (const nlohmann::json::exception& e) {
        spdlog::error(e.what());
    }

    if (m_process == nullptr) {
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

    m_log_parse_errors = true;
    parse_editor_text();
    m_log_parse_errors = false;

    {
        std::ofstream f{m_open_filepath, std::ofstream::out | std::ofstream::binary};

        f.write(m_ui.editor_text.c_str(), m_ui.editor_text.size());
    }

    remember_file();
    save_project();
}

void ReGenny::save_project() {
    std::function<void(nlohmann::json&, const std::string&, node::Property&)> visit =
        [&visit](nlohmann::json& j, const std::string& name, node::Property& prop) {
            if (prop.value.index() == 0 && prop.props.empty()) {
                return;
            }

            if (prop.value != prop.default_value) {
                std::visit(
                    [&](auto&& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, int>) {
                            j[name] = value;
                        } else if constexpr (std::is_same_v<T, bool>) {
                            j[name] = value;
                        }
                    },
                    prop.value);
            }

            for (auto&& [child_name, child_prop] : prop.props) {
                visit(j[name], child_name, child_prop);
            }
        };

    std::function<void(nlohmann::json&)> erase_null = [&erase_null](nlohmann::json& j) {
        if (!j.is_object()) {
            return;
        }

        for (auto it = j.begin(); it != j.end();) {
            if (it->is_null()) {
                it = j.erase(it);
            } else {
                erase_null(it.value());
                ++it;
            }
        }
    };

    m_project["props"].clear();

    try {
        for (auto&& [type_name, props] : m_props) {
            visit(m_project["props"], type_name, props);
        }

        for (auto it = m_project["props"].begin(); it != m_project["props"].end(); ++it) {
            erase_null(it.value());
        }
    } catch (const nlohmann::json::exception& e) {
        spdlog::error(e.what());
    }

    auto proj_filepath = m_open_filepath;

    proj_filepath.replace_extension("json");
    spdlog::info("Saving {}...", proj_filepath.string());

    m_project["chosen_type"] = m_ui.type_name;
    m_project["process_filter"] = m_ui.process_filter;
    m_project["process_id"] = m_ui.process_id;
    m_project["process_name"] = m_ui.process_name;

    std::ofstream f{proj_filepath, std::ofstream::out};

    f << std::setw(4) << m_project;
}

void ReGenny::file_save_as() {
    nfdchar_t* save_path{};

    if (NFD_SaveDialog("genny", m_open_filepath.string().c_str(), &save_path) != NFD_OKAY) {
        return;
    }

    spdlog::info("Saving as {}...", save_path);

    m_open_filepath = save_path;

    file_save();
    free(save_path);
}

void ReGenny::file_quit() {
    SDL_QuitEvent event{};

    event.timestamp = SDL_GetTicks();
    event.type = SDL_QUIT;

    SDL_PushEvent((SDL_Event*)&event);
}

void ReGenny::action_detach() {
    spdlog::info("Detatching...");
    m_process.reset();
    m_mem_ui.reset();
    m_ui.processes.clear();
    SDL_SetWindowTitle(m_window, "ReGenny");
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
    m_sdk->generate(sdk_path);
    free(sdk_path);
}

void ReGenny::attach_ui() {
    if (m_ui.processes.empty()) {
        m_ui.processes = m_helpers->processes();
    }

    ImGui::InputText("Filter", &m_ui.process_filter);

    if (ImGui::BeginListBox("Processes")) {
        for (auto&& [pid, name] : m_ui.processes) {
            if (!m_ui.process_filter.empty()) {
                if (auto it = std::search(name.begin(), name.end(), m_ui.process_filter.begin(),
                        m_ui.process_filter.end(), [](auto a, auto b) { return tolower(a) == tolower(b); });
                    it == name.end()) {
                    continue;
                }
            }
            auto is_selected = pid == m_ui.process_id;

            if (ImGui::Selectable(fmt::format("{} - {}", pid, name).c_str(), is_selected)) {
                m_ui.process_name = name;
                m_ui.process_id = pid;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndListBox();
    }

    if (ImGui::Button("Refresh")) {
        m_ui.processes.clear();
    }

    ImGui::SameLine();

    if (ImGui::Button("Attach")) {
        attach();
    }
}

void ReGenny::attach() {
    if (m_ui.process_id == 0) {
        return;
    }

    // Validate that the chosen process_id and process_name match.
    auto is_valid = false;

    for (auto&& [pid, name] : m_ui.processes) {
        if (pid == m_ui.process_id && name == m_ui.process_name) {
            is_valid = true;
        }
    }

    if (!is_valid) {
        return;
    }

    spdlog::info("Attatching to {} PID: {}...", m_ui.process_name, m_ui.process_id);

    m_process = arch::open_process(m_ui.process_id);

    if (!m_process->ok()) {
        m_ui.error_msg = "Couldn't open the process!";
        ImGui::OpenPopup(m_ui.error_popup);
        m_process.reset();
        return;
    }

    parse_editor_text();
    SDL_SetWindowTitle(m_window, fmt::format("ReGenny - {} PID: {}", m_ui.process_name, m_ui.process_id).c_str());
}

void ReGenny::memory_ui() {
    assert(m_process != nullptr);

    if (ImGui::BeginCombo("Typename", m_ui.type_name.c_str())) {
        for (auto&& type_name : m_ui.type_names) {
            auto is_selected = type_name == m_ui.type_name;

            if (ImGui::Selectable(type_name.c_str(), is_selected)) {
                // Save the previously selected type's props.
                if (m_mem_ui != nullptr) {
                    m_props[m_ui.type_name] = m_mem_ui->props();
                }

                m_ui.type_name = type_name;
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
        return;
    }

    ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "%p", m_address);

    if (m_mem_ui != nullptr) {
        m_mem_ui->display(m_address);
    }
}

void ReGenny::set_address() {
    // We need to access the modules and allocations of the attatched process to determine if the parsed address is
    // valid. If there is no process we can't do that so we just bail.
    if (m_process == nullptr) {
        return;
    }

    auto addr = parse_address(m_ui.address);

    switch (addr.index()) {
    case 1:
        m_address = std::get<uintptr_t>(addr);
        break;
    case 2: {
        auto& modoffset = std::get<ModuleOffset>(addr);
        auto& modname = modoffset.name;

        for (auto&& mod : m_process->modules()) {
            if (std::equal(modname.begin(), modname.end(), mod.name.begin(), mod.name.end(),
                    [](auto a, auto b) { return std::tolower(a) == std::tolower(b); })) {
                m_address = mod.start + modoffset.offset;
                break;
            }
        }

        break;
    }
    default:
        m_address = 0;
        break;
    }

    m_is_address_valid = false;

    for (auto&& allocation : m_process->allocations()) {
        if (allocation.start <= m_address && m_address <= allocation.end) {
            m_is_address_valid = true;
            break;
        }
    }

    remember_type_and_address();
}

void ReGenny::set_type() {
    if (m_sdk == nullptr) {
        return;
    }

    genny::Object* parent = m_sdk->global_ns();
    std::string type_name = m_ui.type_name;
    size_t pos{};

    while ((pos = type_name.find('.')) != std::string::npos) {
        parent = parent->find<genny::Object>(type_name.substr(0, pos));

        if (parent == nullptr) {
            return;
        }

        type_name.erase(0, pos + 1);
    }

    m_type = parent->find<genny::Struct>(type_name);

    if (m_type == nullptr) {
        return;
    }

    if (auto& type_addresses = m_project["type_addresses"]; type_addresses.is_object()) {
        m_ui.address = type_addresses.value(m_ui.type_name, "");
    }

    set_address();

    m_mem_ui =
        std::make_unique<MemoryUi>(*m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_props[m_ui.type_name]);
}

void ReGenny::editor_ui() {
    if (ImGui::InputTextMultiline(
            "##source", &m_ui.editor_text, ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_AllowTabInput)) {
        parse_editor_text();
    }

    if (!m_ui.editor_error_msg.empty()) {
        ImGui::BeginTooltip();
        ImGui::TextColored({1.0f, 0.6f, 0.6f, 1.0f}, "%s", m_ui.editor_error_msg.c_str());
        ImGui::EndTooltip();
    }
}

void ReGenny::parse_editor_text() {
    m_ui.editor_error_msg.clear();

    auto sdk = std::make_unique<genny::Sdk>();

    genny::parser::State s{};
    s.parents.push_back(sdk->global_ns());

    tao::pegtl::memory_input in{m_ui.editor_text, "editor"};

    try {
        if (tao::pegtl::parse<genny::parser::Grammar, genny::parser::Action>(in, s)) {
            m_sdk = std::move(sdk);

            if (m_mem_ui != nullptr) {
                m_props[m_ui.type_name] = m_mem_ui->props();
                m_mem_ui.reset();
            }

            // Build the list of selectable types for the type selector.
            m_ui.type_names.clear();

            std::unordered_set<genny::Struct*> structs{};
            m_sdk->global_ns()->get_all_in_children<genny::Struct>(structs);

            for (auto&& struct_ : structs) {
                std::vector<std::string> parent_names{};

                for (auto p = struct_->owner<genny::Object>(); p != nullptr; p = p->owner<genny::Object>()) {
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
        }
    } catch (const tao::pegtl::parse_error& e) {
        if (m_log_parse_errors) {
            spdlog::error(e.what());
        }
        return;
    }
}

constexpr auto DEFAULT_CFG = R"(
[font]
file = '' 
size = 16

[history]
files = []
)";

void ReGenny::load_cfg() {
    try {
        auto cfg_path = (m_app_path / "cfg.toml").string();
        spdlog::info("Loading config {}...", cfg_path);
        m_cfg = toml::parse_file(cfg_path);
        m_ui.font_to_load = m_cfg["font"]["file"].value_or("");
        m_ui.font_size = m_cfg["font"]["size"].value_or(16);

        if (auto files = m_cfg["history"]["files"].as_array()) {
            for (auto&& file : *files) {
                file.visit([this](auto&& file) noexcept {
                    if constexpr (toml::is_string<decltype(file)>) {
                        m_file_history.emplace_back(*file);
                    }
                });
            }
        }

        if (!m_ui.font_to_load.empty()) {
            m_load_font = true;
        }
    } catch (const toml::parse_error& e) {
        spdlog::error(e.what());

        m_cfg = toml::parse(DEFAULT_CFG);
    }
}

void ReGenny::save_cfg() {
    auto cfg_path = m_app_path / "cfg.toml";

    spdlog::info("Saving config {}...", cfg_path.string());

    std::ofstream f{cfg_path};

    f << m_cfg;
}

void ReGenny::remember_file() {
    for (auto it = m_file_history.begin(); it != m_file_history.end();) {
        if (*it == m_open_filepath) {
            it = m_file_history.erase(it);
        } else {
            ++it;
        }
    }

    m_file_history.emplace_front(m_open_filepath);

    if (m_file_history.size() > 10) {
        m_file_history.resize(10);
    }

    if (auto files = m_cfg["history"]["files"].as_array()) {
        files->clear();

        for (auto&& path : m_file_history) {
            files->push_back(path.string());
        }
    }

    save_cfg();
}

void ReGenny::remember_type_and_address() {
    if (m_type == nullptr || !m_is_address_valid) {
        return;
    }

    m_project["type_addresses"][m_ui.type_name] = m_ui.address;
}
