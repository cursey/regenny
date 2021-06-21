#include <algorithm>
#include <cassert>
#include <cstdlib>

#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <fmt/format.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nfd.h>
#include <spdlog/spdlog.h>

#include "Utility.hpp"
#include "arch/Arch.hpp"
#include "imgui_freetype.h"

#include "ReGenny.hpp"

/*constexpr auto DEFAULT_EDITOR_TEXT = R"(
type USHORT 2 [[u16]]
type LONG 4   [[i32]]

struct IMAGE_DOS_HEADER
    USHORT e_magic
    USHORT e_cblp
    USHORT e_cp
    USHORT e_crlc
    USHORT e_cparhdr
    USHORT e_minalloc
    USHORT e_maxalloc
    USHORT e_ss
    USHORT e_sp
    USHORT e_csum
    USHORT e_ip
    USHORT e_cs
    USHORT e_lfarlc
    USHORT e_ovno
    USHORT[4] e_res
    USHORT e_oemid
    USHORT e_oeminfo
    USHORT[10] e_res2
    LONG e_lfanew
)";

constexpr auto DEFAULT_EDITOR_TYPE = "IMAGE_DOS_HEADER";
*/

constexpr auto DEFAULT_EDITOR_TEXT = R"(
type int 4 [[i32]]
type float 4 [[f32]]
type ushort 2 [[u16]]
type str 8 [[utf8*]]
type wstr 8 [[utf16*]]

enum Place
    EARTH = 1
    MOON = 2
    MARS = 3

struct Date
    ushort nWeekDay : 3
    ushort nMonthDay : 6
    ushort nMonth : 5
    ushort nYear : 8

struct Foo
    int a
    int b
    float c
    Place p

struct Bar
    int d
    Foo* foo
    int[4][3] m
    Date date

struct Thing 
    int abc

struct Baz : Bar
    int e
    int* f
    Foo g
    Thing* things
    str hello
    wstr wide_hello
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
    int* f{};
    Foo g{};
    Thing* things{};
    char* hello{};
    wchar_t* wide_hello{};
};
#include <poppack.h>

ReGenny::ReGenny() {
    m_actions[Action::OPEN] = (thor::Action(sf::Keyboard::LControl) || thor::Action(sf::Keyboard::RControl)) &&
                              thor::Action(sf::Keyboard::O, thor::Action::PressOnce);
    m_actions[Action::SAVE] = (thor::Action(sf::Keyboard::LControl) || thor::Action(sf::Keyboard::RControl)) &&
                              thor::Action(sf::Keyboard::S, thor::Action::PressOnce);
    m_actions[Action::QUIT] = (thor::Action(sf::Keyboard::LControl) || thor::Action(sf::Keyboard::RControl)) &&
                              thor::Action(sf::Keyboard::Q, thor::Action::PressOnce);
    m_actions_system.connect0(Action::OPEN, [this] { file_open(); });
    m_actions_system.connect0(Action::SAVE, [this] { file_save(); });
    m_actions_system.connect0(Action::QUIT, [this] { m_window.close(); });

    spdlog::set_default_logger(m_logger.logger());
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::info("Hello, world!");
    m_window.setFramerateLimit(60);
    ImGui::SFML::Init(m_window);

    m_helpers = arch::make_helpers();
    m_ui.processes = m_helpers->processes();

    // Defaults for testing.
    m_ui.editor_text = DEFAULT_EDITOR_TEXT;
    m_ui.type_name = DEFAULT_EDITOR_TYPE;
    m_ui.process_id = GetCurrentProcessId();
    m_ui.process_name = "ReGenny.exe";
    // m_ui.address = "<ReGenny.exe>+0x0";

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

    // Just for testing...
    /*auto ns = m_sdk.global_ns();

    ns->type("USHORT")->size(2);
    ns->type("LONG")->size(4);

    auto dos = ns->struct_("IMAGE_DOS_HEADER");

    dos->variable("e_magic")->offset(0)->type("USHORT");
    dos->variable("e_cblp")->offset(2)->type("USHORT");
    dos->variable("e_cp")->offset(4)->type("USHORT");
    dos->variable("e_crlc")->offset(6)->type("USHORT");
    dos->variable("e_cparhdr")->offset(8)->type("USHORT");
    dos->variable("e_minalloc")->offset(10)->type("USHORT");
    dos->variable("e_maxalloc")->offset(12)->type("USHORT");
    dos->variable("e_ss")->offset(14)->type("USHORT");
    dos->variable("e_sp")->offset(16)->type("USHORT");
    dos->variable("e_csum")->offset(18)->type("USHORT");
    dos->variable("e_ip")->offset(20)->type("USHORT");
    dos->variable("e_cs")->offset(22)->type("USHORT");
    dos->variable("e_lfarlc")->offset(24)->type("USHORT");
    dos->variable("e_ovno")->offset(26)->type("USHORT");
    dos->array_("e_res")->count(4)->offset(28)->type("USHORT");
    dos->variable("e_oemid")->offset(36)->type("USHORT");
    dos->variable("e_oeminfo")->offset(38)->type("USHORT");
    dos->array_("e_res2")->count(10)->offset(40)->type("USHORT");
    dos->variable("e_lfanew")->offset(60)->type("LONG");

    auto bf = dos->bitfield(64);

    bf->type("USHORT");
    bf->field("nWeekDay")->offset(0)->size(3);
    bf->field("nMonthDay")->offset(3)->size(6);
    bf->field("nMonth")->offset(bf->field("nMonthDay")->end())->size(5);
    bf->field("nYear")->offset(16)->size(8);

    m_type = dos;*/

    set_type();
}

ReGenny::~ReGenny() {
    ImGui::SFML::Shutdown();
}

void ReGenny::run() {
    sf::Clock delta_clock{};

    while (m_window.isOpen()) {
        m_actions.clearEvents();

        sf::Event evt{};

        while (m_window.pollEvent(evt)) {
            m_actions.pushEvent(evt);
            ImGui::SFML::ProcessEvent(evt);

            if (evt.type == sf::Event::Closed) {
                m_window.close();
            }
        }

        m_actions.invokeCallbacks(m_actions_system, &m_window);

        if (m_load_font) {
            spdlog::info("Setting font {}...", m_ui.font_to_load);

            auto& io = ImGui::GetIO();
            io.Fonts->Clear();
            io.Fonts->AddFontFromFileTTF(m_ui.font_to_load.c_str(), m_ui.font_size);
            ImGuiFreeType::BuildFontAtlas(io.Fonts, 0);
            ImGui::SFML::UpdateFontTexture();
            m_load_font = false;
        }

        ImGui::SFML::Update(m_window, delta_clock.restart());
        ui();

        m_window.clear();
        ImGui::SFML::Render(m_window);
        m_window.display();
    }
}

void ReGenny::ui() {
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)m_window.getSize().x, (float)m_window.getSize().y}, ImGuiCond_Always);
    ImGui::Begin("ReGenny", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);

    // Resizing gets kinda wonky because the ImGui window will try to resize more often than the actual window (atleast
    // on Windows 10). If we add the ImGuiWindowFlags_NoResize option to the window flags above, the window becomes even
    // harder to resize for some reason. So we fix that by letting the ImGui resize normally and then just setting the
    // actual window size to whatever the ImGui window size is.
    sf::Vector2u winsize{(unsigned int)ImGui::GetWindowWidth(), (unsigned int)ImGui::GetWindowHeight()};

    if (winsize != m_window_size) {
        m_window.setSize(winsize);
        m_window_size = m_window.getSize();
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

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                file_save();
            }

            if (ImGui::MenuItem("Save As...")) {
                file_save_as();
            }

            if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                m_window.close();
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

        ImGui::EndMenuBar();
    }
}

void ReGenny::file_open() {
    nfdchar_t* out_path{};

    if (NFD_OpenDialog("genny", nullptr, &out_path) != NFD_OKAY) {
        return;
    }

    spdlog::info("Opening {}...", out_path);

    std::ifstream f{out_path, std::ifstream::in | std::ifstream::binary | std::ifstream::ate};

    m_ui.editor_text.resize(f.tellg());
    f.seekg(0, std::ifstream::beg);
    f.read(m_ui.editor_text.data(), m_ui.editor_text.size());

    m_open_filename = out_path;
    free(out_path);

    m_log_parse_errors = true;
    parse_editor_text();
    m_log_parse_errors = false;
}

void ReGenny::file_save() {
    if (m_open_filename.empty()) {
        file_save_as();
        return;
    }

    spdlog::info("Saving {}...", m_open_filename);

    m_log_parse_errors = true;
    parse_editor_text();
    m_log_parse_errors = false;

    std::ofstream f{m_open_filename, std::ofstream::out | std::ofstream::binary};

    f.write(m_ui.editor_text.c_str(), m_ui.editor_text.size());
}

void ReGenny::file_save_as() {
    nfdchar_t* save_path{};

    if (NFD_SaveDialog("genny", m_open_filename.c_str(), &save_path) != NFD_OKAY) {
        return;
    }

    spdlog::info("Saving as {}...", save_path);

    m_open_filename = save_path;

    file_save();
    free(save_path);
}

void ReGenny::action_detach() {
    spdlog::info("Detatching...");
    m_process.reset();
    m_mem_ui.reset();
    m_window.setTitle("ReGenny");
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
    if (ImGui::BeginListBox("Processes")) {
        for (auto&& [name, pid] : m_ui.processes) {
            if (ImGui::Selectable(fmt::format("{} - {}", pid, name).c_str(), pid == m_ui.process_id)) {
                m_ui.process_name = name;
                m_ui.process_id = pid;
            }
        }

        ImGui::EndListBox();
    }

    if (ImGui::Button("Refresh")) {
        m_ui.processes = m_helpers->processes();
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

    m_modules = m_process->modules();

    parse_editor_text();
    set_address();
    set_type();

    m_window.setTitle(fmt::format("ReGenny - {} PID: {}", m_ui.process_name, m_ui.process_id));
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

    if (ImGui::InputText("Typename", &m_ui.type_name)) {
        set_type();
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

        for (auto&& mod : m_modules) {
            auto name = mod->name();

            if (std::equal(modname.begin(), modname.end(), name, name + strlen(name),
                    [](auto a, auto b) { return std::tolower(a) == std::tolower(b); })) {
                m_address = mod->address() + modoffset.offset;
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
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl)) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) && !m_ui.editor_has_saved) {
            file_save();
            m_ui.editor_has_saved = true;
        }
    } else {
        m_ui.editor_has_saved = false;
    }

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
    s.global_ns = s.cur_ns = sdk->global_ns();

    tao::pegtl::memory_input in{m_ui.editor_text, "editor"};

    try {
        if (tao::pegtl::parse<genny::parser::Grammar, genny::parser::Action>(in, s)) {
            m_sdk = std::move(sdk);

            if (m_mem_ui != nullptr) {
                m_inherited_props = m_mem_ui->props();
            }

            m_mem_ui.reset();
            set_type();
        }
    } catch (const tao::pegtl::parse_error& e) {
        if (m_log_parse_errors) {
            spdlog::error(e.what());
        }
        return;
    }
}
