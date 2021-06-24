#include <algorithm>
#include <cassert>
#include <cstdlib>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <nfd.h>
#include <spdlog/spdlog.h>

#include "AboutUi.hpp"
#include "Utility.hpp"
#include "arch/Arch.hpp"

#include "ReGenny.hpp"

constexpr auto DEFAULT_EDITOR_TEXT = R"(
type int 4 [[i32]]
type float 4 [[f32]]
type ushort 2 [[u16]]
type str 8 [[utf8*]]
type wstr 8 [[utf16*]]
type bool 1 [[bool]]

enum Place {
    EARTH = 1,
    MOON = 2,
    MARS = 3,
}

struct Date {
    ushort nWeekDay : 3
    ushort nMonthDay : 6
    ushort nMonth : 5
    ushort nYear : 8
}

struct Foo {
    int a
    int b
    float c
    Place p
}

struct Bar {
    int d
    Foo* foo
    int[4][3] m
    Date date
}

struct Thing {
    int abc
}

struct Baz : Bar {
    int e
    int thing
    int* f
    Foo g
    Thing* things
    str hello
    wstr wide_hello
    bool im_true
    bool im_false
    bool im_also_true
}
)";

constexpr auto DEFAULT_EDITOR_TYPE = "Baz";

#include <pshpack1.h>
enum Place { EARTH = 1, MOON = 2, MARS = 3 };

struct Date {
    unsigned short nWeekDay : 3;
    unsigned short nMonthDay : 6;
    unsigned short nMonth : 5;
    unsigned short nYear : 8;
};

struct Foo {
    int a{};
    int b{};
    float c{};
    Place p{};
};

struct Bar {
    int d{};
    Foo* foo{};
    int m[4][3];
    union {
        Date date;
        unsigned int date_int{};
    };
};

struct Thing {
    int abc{};
};

struct Baz : Bar {
    int e{};
    int thing{};
    int* f{};
    Foo g{};
    Thing* things{};
    char* hello{};
    wchar_t* wide_hello{};
    bool im_true{true};
    bool im_false{false};
    char im_also_true{7};
};
#include <poppack.h>

ReGenny::ReGenny(SDL_Window* window) : m_window{window} {
    spdlog::set_default_logger(m_logger.logger());
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::info("Hello, world!");

    auto path_str = SDL_GetPrefPath("cursey", "ReGenny");
    m_app_path = path_str;
    SDL_free(path_str);

    load_cfg();

    m_triggers.on({SDLK_LCTRL, SDLK_o}, [this] { file_open(); });
    m_triggers.on({SDLK_LCTRL, SDLK_s}, [this] { file_save(); });
    m_triggers.on({SDLK_LCTRL, SDLK_q}, [this] { file_quit(); });

    m_helpers = arch::make_helpers();

    // Defaults for testing.
    m_ui.editor_text = DEFAULT_EDITOR_TEXT;
    m_ui.type_name = DEFAULT_EDITOR_TYPE;
    m_ui.process_id = GetCurrentProcessId();
    m_ui.process_name = "ReGenny.exe";

    auto foo = new Foo{};
    foo->a = 42;
    foo->b = 1337;
    foo->c = 77.7f;
    foo->p = Place::MARS;

    auto baz = new Baz{};
    baz->d = 123;
    baz->foo = foo;
    for (auto i = 0; i < 4; ++i) {
        for (auto j = 0; j < 3; ++j) {
            baz->m[i][j] = i + j;
        }
    }
    baz->date.nWeekDay = 1;
    baz->date.nMonthDay = 2;
    baz->date.nMonth = 3;
    baz->date.nYear = 4;
    baz->e = 666;
    baz->f = new int[10];
    for (auto i = 0; i < 10; ++i) {
        baz->f[i] = i;
    }
    baz->g = *foo;
    ++baz->g.a;
    ++baz->g.b;
    ++baz->g.c;
    baz->g.p = Place::MOON;
    baz->things = new Thing[10];
    for (auto i = 0; i < 10; ++i) {
        baz->things[i].abc = i * 2;
    }
    baz->hello = "Hello, world!";
    baz->wide_hello = L"Hello, wide world!";

    m_ui.address = fmt::format("0x{:X}", (uintptr_t)baz);

    attach();
    set_address();
    parse_editor_text();
    // set_type();
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
    ImGui::Begin("ReGenny", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);

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

    auto h = ImGui::GetContentRegionAvail().y * (1.61f * 0.5f);

    if (m_process == nullptr) {
        ImGui::BeginChild("attach", ImVec2{0.0f, h});
        attach_ui();
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("memview", ImVec2{ImGui::GetWindowContentRegionWidth() / 1.61f, h});
        memory_ui();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("editor", ImVec2{0.0f, h});
        editor_ui();
        ImGui::EndChild();
    }

    m_logger.ui();

    // ImGui::ShowDemoWindow();

    m_ui.error_popup = ImGui::GetID("Error");
    if (ImGui::BeginPopupModal("Error")) {
        ImGui::Text(m_ui.error_msg.c_str());

        if (ImGui::Button("Ok")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

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

    m_ui.editor_text.resize(f.tellg());
    f.seekg(0, std::ifstream::beg);
    f.read(m_ui.editor_text.data(), m_ui.editor_text.size());

    m_log_parse_errors = true;
    parse_editor_text();
    m_log_parse_errors = false;

    remember_file();
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

    std::ofstream f{m_open_filepath, std::ofstream::out | std::ofstream::binary};

    f.write(m_ui.editor_text.c_str(), m_ui.editor_text.size());

    remember_file();
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

    if (ImGui::BeginListBox("Processes")) {
        for (auto&& [pid, name] : m_ui.processes) {
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

    spdlog::info("Attatching to {} PID: {}...", m_ui.process_name, m_ui.process_id);

    m_process = arch::open_process(m_ui.process_id);

    if (!m_process->ok()) {
        m_ui.error_msg = "Couldn't open the process!";
        ImGui::OpenPopup(m_ui.error_popup);
        m_process.reset();
        return;
    }

    parse_editor_text();
    set_address();
    set_type();

    SDL_SetWindowTitle(m_window, fmt::format("ReGenny - {} PID: {}", m_ui.process_name, m_ui.process_id).c_str());
}

void ReGenny::memory_ui() {
    assert(m_process != nullptr);

    if (ImGui::InputText("Address", &m_ui.address)) {
        set_address();
    }

    ImGui::SameLine();

    if (m_address == 0) {
        ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Invalid address!");
        return;
    } else {
        ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "%p", m_address);
    }

    /*if (ImGui::InputText("Typename", &m_ui.type_name)) {
        set_type();
    }*/

    if (ImGui::BeginCombo("Typename", m_ui.type_name.c_str())) {
        for (auto&& type_name : m_ui.type_names) {
            auto is_selected = type_name == m_ui.type_name;

            if (ImGui::Selectable(type_name.c_str(), is_selected)) {
                m_ui.type_name = type_name;
                set_type();
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    if (m_mem_ui != nullptr) {
        m_mem_ui->display();
    }
}

void ReGenny::set_address() {
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
            }
        }

        break;
    }
    default:
        m_address = 0;
        break;
    }

    if (m_mem_ui != nullptr) {
        m_inherited_props = m_mem_ui->props();
    }

    m_mem_ui = std::make_unique<MemoryUi>(
        *m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_address, m_inherited_props);
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

    if (m_mem_ui != nullptr) {
        m_inherited_props = m_mem_ui->props();
    }

    m_mem_ui = std::make_unique<MemoryUi>(
        *m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_address, m_inherited_props);
}

void ReGenny::editor_ui() {
    if (ImGui::InputTextMultiline(
            "##source", &m_ui.editor_text, ImGui::GetWindowContentRegionMax(), ImGuiInputTextFlags_AllowTabInput)) {
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
                m_inherited_props = m_mem_ui->props();
            }

            m_mem_ui.reset();

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
        spdlog::warn(e.what());

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
