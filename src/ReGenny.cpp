#include <algorithm>
#include <cassert>
#include <cstdlib>

#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <fmt/format.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Utility.hpp"
#include "arch/Arch.hpp"

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

struct Foo
    int a
    int b
    float c

struct Bar
    int a
    Foo* foo
    int[4][3] m
)";

constexpr auto DEFAULT_EDITOR_TYPE = "Bar";

#include <pshpack1.h>
struct Foo {
    int a{};
    int b{};
    float c{};
};

struct Bar {
    int a{};
    Foo* foo{};
    int m[4][3];
};
#include <poppack.h>

ReGenny::ReGenny() {
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

    auto bar = new Bar{};
    bar->a = 123;
    bar->foo = foo;
    for (auto i = 0; i < 4; ++i) {
        for (auto j = 0; j < 3; ++j) {
            bar->m[i][j] = i + j;
        }
    }

    m_ui.address = fmt::format("0x{:X}", (uintptr_t)bar);

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
        sf::Event evt{};

        while (m_window.pollEvent(evt)) {
            ImGui::SFML::ProcessEvent(evt);

            if (evt.type == sf::Event::Closed) {
                m_window.close();
            }
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
    ImGui::Begin("ReGenny", nullptr, ImGuiWindowFlags_NoTitleBar);

    // Resizing gets kinda wonky because the ImGui window will try to resize more often than the actual window (atleast
    // on Windows 10). If we add the ImGuiWindowFlags_NoResize option to the window flags above, the window becomes even
    // harder to resize for some reason. So we fix that by letting the ImGui resize normally and then just setting the
    // actual window size to whatever the ImGui window size is.
    sf::Vector2u winsize{(unsigned int)ImGui::GetWindowWidth(), (unsigned int)ImGui::GetWindowHeight()};

    if (winsize != m_window_size) {
        m_window.setSize(winsize);
        m_window_size = m_window.getSize();
    }

    if (m_process == nullptr) {
        attach_ui();
    } else {
        ImGui::BeginChild("memview", ImVec2{ImGui::GetWindowContentRegionWidth() * 0.66f, 0});
        memory_ui();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("editor");
        editor_ui();
        ImGui::EndChild();
    }

    // ImGui::ShowDemoWindow();

    ImGui::End();

    if (ImGui::BeginPopupModal("Error")) {
        ImGui::Text(m_ui.error_msg.c_str());

        if (ImGui::Button("Ok")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
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

    m_process = arch::open_process(m_ui.process_id);

    if (!m_process->ok()) {
        m_ui.error_msg = "Couldn't open the process!";
        ImGui::OpenPopup("Error");
        return;
    }

    m_modules = m_process->modules();

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
        m_mem_ui = std::make_unique<MemoryUi>(*m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_address);
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

        m_mem_ui = std::make_unique<MemoryUi>(*m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_address);
        break;
    }
    default:
        m_address = 0;
        break;
    }
}

void ReGenny::set_type() {
    m_type = m_sdk->global_ns()->find<genny::Struct>(m_ui.type_name);

    if (m_type == nullptr) {
        return;
    }

    m_mem_ui = std::make_unique<MemoryUi>(*m_sdk, dynamic_cast<genny::Struct*>(m_type), *m_process, m_address);
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
    s.global_ns = s.cur_ns = sdk->global_ns();

    tao::pegtl::memory_input in{m_ui.editor_text, "editor"};

    try {
        if (tao::pegtl::parse<genny::parser::Grammar, genny::parser::Action>(in, s)) {
            m_sdk = std::move(sdk);
            set_type();
        }
    } catch (const tao::pegtl::parse_error& e) {
        m_ui.editor_error_msg = e.what();
        return;
    }
}
