#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <regex>

#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <fmt/format.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "ReGenny.hpp"
#include "arch/Arch.hpp"

ReGenny::ReGenny() {
    m_window.setFramerateLimit(60);
    ImGui::SFML::Init(m_window);

    m_helpers = arch::make_helpers();
    m_ui.processes = m_helpers->processes();

    // Just for testing...
    auto ns = m_sdk.global_ns();

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

    m_type = dos;

    for (auto&& var : m_type->get_all<genny::Variable>()) {
        m_var_map[var->offset()] = var;
    }

    m_ui.type_name = "IMAGE_DOS_HEADER";
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
        memory_ui();
    }

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

    // Handle refreshing our view of the memory automatically every half a second.
    auto now = std::chrono::steady_clock::now();

    if (now >= m_next_refresh_time) {
        refresh_memory();
        m_next_refresh_time = now + std::chrono::milliseconds{500};
    }


    if (ImGui::InputText("Address", &m_ui.address)) {
        std::regex module_offset_regex{R"(<(.+)>\+([^0][0-9]+|0[xX][0-9a-fA-F]+))"};
        std::smatch matches{};

        if (std::regex_match(m_ui.address, matches, module_offset_regex)) {
            auto modname = matches[1].str();
            auto offset = matches[2].str();

            for (auto&& mod : m_modules) {
                auto name = mod->name();

                if (std::equal(modname.begin(), modname.end(), name, name + strlen(name),
                        [](auto a, auto b) { return std::tolower(a) == std::tolower(b); })) {
                    m_address = (uintptr_t)std::strtoull(offset.c_str(), nullptr, 0);
                    m_address += mod->address();
                }
            }
        } else {
            m_address = (uintptr_t)std::strtoull(m_ui.address.c_str(), nullptr, 0);
        }
    }

    ImGui::SameLine();

    if (m_address == 0) {
        ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Invalid address!");
        return;
    } else {
        ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "%p", m_address);
    }

    if (ImGui::InputText("Typename", &m_ui.type_name)) {
    }

    if (auto search = m_mem.find(m_address); search == m_mem.end()) {
        return;
    }

    auto& cur_mem = m_mem[m_address];

    ImGui::Text("%16s %8s %s", "Address", "Offset", "Data");

    for (auto i = 0; i < cur_mem.size();) {
        if (auto search = m_var_map.find(i); search != m_var_map.end()) {
            auto var = search->second;
            static std::string var_decl{};

            var_decl = var->name();

            if (auto arr = dynamic_cast<genny::Array*>(var)) {
                var_decl += fmt::format("[{}]", arr->count());
            }

            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%016llX", m_address + i);
            ImGui::SameLine();
            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%8X", i);
            ImGui::SameLine();
            ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, var->type()->name().c_str());
            ImGui::SameLine();
            ImGui::Text("%s =", var_decl.c_str());
            ImGui::SameLine();
            draw_variable_value(var, cur_mem);

            i += var->size();
        } else if (i % sizeof(uintptr_t) != 0) {
            i = (i + sizeof(uintptr_t) - 1) / sizeof(uintptr_t) * sizeof(uintptr_t);
        } else {
            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%016llX", m_address + i);
            ImGui::SameLine();
            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%8X", i);
            ImGui::SameLine();
            ImGui::Text("%p", *(uintptr_t*)&cur_mem[i]);
            i += sizeof(uintptr_t);
        }
    }
}

void ReGenny::refresh_memory() {
    assert(m_process != nullptr);

    if (auto search = m_mem.find(m_address); search == m_mem.end()) {
        m_mem.clear();
        m_mem[m_address] = std::vector<std::byte>(0x1000, std::byte{});
    }

    for (auto&& [address, buffer] : m_mem) {
        m_process->read(address, buffer.data(), buffer.size());
    }
}

void ReGenny::draw_variable_value(genny::Variable* var, const std::vector<std::byte>& mem) {
    auto draw_var_internal = [&mem](size_t size, uintptr_t offset) {
        switch (size) { 
        case 1:
            ImGui::Text("%02X", *(uint8_t*)&mem[offset]);
            break;
        case 2:
            ImGui::Text("%04X", *(uint16_t*)&mem[offset]);
            break;
        case 4:
            ImGui::Text("%08X", *(uint32_t*)&mem[offset]);
            break;
        case 8:
            ImGui::Text("%016llX", *(uint32_t*)&mem[offset]);
            break;
        default:
            static std::string s{};

            s.clear();

            for (auto j = 0; j < size; ++j) {
                s += fmt::format("{:02X} ", *(uint8_t*)&mem[offset]);
            }

            ImGui::Text(s.c_str());
        }
    };

    if (auto arr = dynamic_cast<genny::Array*>(var)) {
        auto element_size = arr->type()->size();
        auto num_elements = arr->count();

        for (auto i = 0; i < num_elements; ++i) {
            draw_var_internal(element_size, arr->offset() + i * element_size);

            if (i != num_elements - 1) {
                ImGui::SameLine();
            }
        }
    } else {
        draw_var_internal(var->size(), var->offset());
    }
}
