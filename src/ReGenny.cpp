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
#include "GennyParser.hpp"
#include "Utility.hpp"
#include "arch/Arch.hpp"
#include "node/Undefined.hpp"

#include "ReGenny.hpp"

using namespace std::literals;

ReGenny::ReGenny(SDL_Window* window)
    : m_window{window}, m_helpers{arch::make_helpers()}, m_process{std::make_unique<Process>()} {
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
    if (m_reload_file) {
        file_reload();
    }

    // Auto detach from closed processes.
    if (m_process != nullptr) {
        if (!m_process->ok()) {
            action_detach();
        }
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

        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Up, 1.61f * 0.5f, &top, &bottom);
        ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.66f, &left, &right);

        ImGui::DockBuilderDockWindow("Attach", left);
        ImGui::DockBuilderDockWindow("Memory View", left);
        ImGui::DockBuilderDockWindow("Editor", right);
        ImGui::DockBuilderDockWindow("Log", bottom);

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

    ImGui::Begin("Memory View");
    memory_ui();
    ImGui::End();

    ImGui::Begin("Editor");
    editor_ui();
    ImGui::End();

    ImGui::Begin("Log");
    m_logger.ui();
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
            ImGui::Checkbox("Reload current .genny file on changes", &m_reload_file);
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
    if (m_open_filepath.empty()) {
        return;
    }
    auto cwt = std::filesystem::last_write_time(m_open_filepath);
    if (cwt != m_file_lwt) {
        spdlog::info("Reopening {}...", m_open_filepath.string());

        std::ifstream f{m_open_filepath, std::ifstream::in | std::ifstream::binary | std::ifstream::ate};

        if (!f) {
            spdlog::error("Failed to open {}!", m_open_filepath.string());
            return;
        }

        m_ui.editor_text.resize(f.tellg());
        f.seekg(0, std::ifstream::beg);
        f.read(m_ui.editor_text.data(), m_ui.editor_text.size());

        m_log_parse_errors = true;
        parse_editor_text();
        m_log_parse_errors = false;
    }
    m_file_lwt = cwt;
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
    spdlog::info("Detaching...");
    m_process = std::make_unique<Process>();
    m_mem_ui = std::make_unique<MemoryUi>(
        m_cfg, *m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_project.props[m_project.type_chosen]);
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
        m_ui.error_msg = "Couldn't open the process!";
        ImGui::OpenPopup(m_ui.error_popup);
        m_process.reset();
        return;
    }

    parse_editor_text();
    SDL_SetWindowTitle(
        m_window, fmt::format("ReGenny - {} PID: {}", m_project.process_name, m_project.process_id).c_str());
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

            const auto demangled = tname->substr(4, tname->find_first_of("@@") - 4);

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
    // We need to access the modules and allocations of the attached process to determine if the parsed address is
    // valid. If there is no process we can't do that, so we just bail.
    if (m_process == nullptr) {
        return;
    }

    if (auto addr = parse_address(m_ui.address)) {
        m_parsed_address = *addr;
    }

    remember_type_and_address();
}

void ReGenny::set_type() {
    if (m_sdk == nullptr) {
        return;
    }

    genny::Object* parent = m_sdk->global_ns();
    std::string type_name = m_project.type_chosen;
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

    if (auto search = m_project.type_addresses.find(m_project.type_chosen); search != m_project.type_addresses.end()) {
        m_ui.address = search->second;
    } else {
        m_ui.address.clear();
    }

    set_address();

    m_mem_ui = std::make_unique<MemoryUi>(
        m_cfg, *m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_project.props[m_project.type_chosen]);
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
    s.filepath = m_open_filepath;
    s.parents.push_back(sdk->global_ns());

    tao::pegtl::memory_input in{m_ui.editor_text, "editor"};

    try {
        if (tao::pegtl::parse<genny::parser::Grammar, genny::parser::Action>(in, s)) {
            m_sdk = std::move(sdk);

            if (m_mem_ui != nullptr) {
                m_project.props[m_project.type_chosen] = m_mem_ui->props();
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

void ReGenny::load_cfg() {
    auto cfg_path = (m_app_path / "cfg.json").string();

    spdlog::info("Loading config {}...", cfg_path);

    try {
        std::ifstream f{cfg_path};
        nlohmann::json j{};

        f >> j;
        m_cfg = j.get<Config>();

        if (!m_cfg.font_file.empty()) {
            m_load_font = true;
        }
    } catch (const nlohmann::json::exception& e) {
        spdlog::error(e.what());
        m_cfg = {};
    }
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
