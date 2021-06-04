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

constexpr auto DEFAULT_EDITOR_TEXT = R"(
type USHORT 2
type LONG 4

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

ReGenny::ReGenny() {
    m_window.setFramerateLimit(60);
    ImGui::SFML::Init(m_window);

    m_helpers = arch::make_helpers();
    m_ui.processes = m_helpers->processes();

    m_ui.editor_text = DEFAULT_EDITOR_TEXT;
    m_ui.type_name = "IMAGE_DOS_HEADER";

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

    // Handle refreshing our view of the memory automatically every half a second.
    auto now = std::chrono::steady_clock::now();

    if (now >= m_next_refresh_time) {
        refresh_memory();
        m_next_refresh_time = now + std::chrono::milliseconds{500};
    }

    if (ImGui::InputText("Address", &m_ui.address)) {
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

    if (auto search = m_mem.find(m_address); search == m_mem.end()) {
        return;
    }

    auto& cur_mem = m_mem[m_address];

    ImGui::Text("%16s %8s %s", "Address", "Offset", "Data");

    for (auto i = 0; i < cur_mem.size();) {
        if (auto search = m_var_map.find(i); search != m_var_map.end()) {
            auto var = search->second;

            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%016llX", m_address + i);
            ImGui::SameLine();
            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%8X", i);
            ImGui::SameLine();
            draw_variable(var, cur_mem);

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

    // Keep a list of addresses we visit so we can remove unvisted memory buffers from m_mem.
    std::unordered_set<uintptr_t> visited_addresses{};

    if (auto search = m_mem.find(m_address); search == m_mem.end()) {
        m_mem[m_address] = std::vector<std::byte>(m_type->size() + 0x1000, std::byte{});
    }

    visited_addresses.emplace(m_address);

    // Visit pointers will walk through all variables for a given type and if it encounters a pointer it will create an
    // entry in m_mem for it so that its memory can be refreshed.
    std::function<void(genny::Type*, uintptr_t)> visit_pointers = [&](genny::Type* type, uintptr_t address) {
        for (auto&& var : type->get_all<genny::Variable>()) {
            if (auto ptr = dynamic_cast<genny::Pointer*>(var->type())) {
                auto var_address = *(uintptr_t*)&m_mem[address][var->offset()];

                if (auto search = m_mem.find(var_address); search == m_mem.end()) {
                    m_mem[var_address] = std::vector<std::byte>(ptr->to()->size());
                    visited_addresses.emplace(var_address);
                }

                visit_pointers(ptr->to(), var_address);
            }
        }
    };

    visit_pointers(m_type, m_address);

    // Remove unvisted memory buffers.
    for (auto it = m_mem.begin(); it != m_mem.end();) {
        if (auto search = visited_addresses.find(it->first); search == visited_addresses.end()) {
            it = m_mem.erase(it);
        } else {
            ++it;
        }
    }

    // Refresh the memory of all visited memory buffers.
    for (auto&& [address, buffer] : m_mem) {
        m_process->read(address, buffer.data(), buffer.size());
    }
}

void ReGenny::draw_variable(genny::Variable* var, const std::vector<std::byte>& mem) {
    if (var == nullptr) {
        return;
    }

    auto draw_val = [&mem](size_t size, uintptr_t offset) {
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

            ImGui::TextUnformatted(s.c_str());
        }
    };

    auto draw_bin = [&mem](uintptr_t offset, size_t bit_start, size_t bit_end) {
        static std::string s{};

        s = "0b";

        for (auto i = bit_start; i < bit_end; ++i) {
            auto byte = *(uint8_t*)&mem[offset + i / CHAR_BIT];
            auto bit = byte >> (i % CHAR_BIT) & 1;

            if (bit) {
                s += "1";
            } else {
                s += "0";
            }
        }

        ImGui::TextUnformatted(s.c_str());
    };

    if (auto bf = dynamic_cast<genny::Bitfield*>(var)) {
        static std::map<uintptr_t, genny::Bitfield::Field*> field_map{};

        field_map.clear();

        for (auto&& field : bf->get_all<genny::Bitfield::Field>()) {
            field_map[field->offset()] = field;
        }

        size_t offset = 0;
        auto max_offset = bf->size() * CHAR_BIT;
        auto last_offset = offset;

        auto cur_x = ImGui::GetCursorPosX();

        while (offset < max_offset) {
            if (auto search = field_map.find(offset); search != field_map.end()) {
                auto field = search->second;

                // Skip unfinished fields.
                if (field->size() == 0) {
                    ++offset;
                    continue;
                }

                if (offset - last_offset > 0) {
                    ImGui::SetCursorPosX(cur_x);
                    ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, bf->type()->name().c_str());
                    ImGui::SameLine();
                    ImGui::Text("%s_pad_%x : %d =", bf->name().c_str(), last_offset, offset - last_offset);
                    ImGui::SameLine();
                    draw_bin(bf->offset(), last_offset, offset);
                }

                ImGui::SetCursorPosX(cur_x);
                ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, bf->type()->name().c_str());
                ImGui::SameLine();
                ImGui::Text("%s : %d =", field->name().c_str(), field->size());
                ImGui::SameLine();
                draw_bin(bf->offset(), offset, offset + field->size());
                offset += field->size();
                last_offset = offset;
            } else {
                ++offset;
            }
        }
    } else if (auto arr = dynamic_cast<genny::Array*>(var)) {
        ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, arr->type()->name().c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(fmt::format("{} [{}] =", arr->name(), arr->count()).c_str());
        ImGui::SameLine();

        auto element_size = arr->type()->size();
        auto num_elements = arr->count();

        for (auto i = 0; i < num_elements; ++i) {
            draw_val(element_size, arr->offset() + i * element_size);

            if (i != num_elements - 1) {
                ImGui::SameLine();
            }
        }
    } else {
        ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, var->type()->name().c_str());
        ImGui::SameLine();
        ImGui::Text("%s =", var->name().c_str());
        ImGui::SameLine();
        draw_val(var->size(), var->offset());
    }
}

void ReGenny::set_type() {
    m_type = m_sdk->global_ns()->find<genny::Struct>(m_ui.type_name);

    if (m_type == nullptr) {
        return;
    }

    m_var_map.clear();

    for (auto&& var : m_type->get_all<genny::Variable>()) {
        m_var_map[var->offset()] = var;
    }
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
            m_var_map.clear();
            m_sdk = std::move(sdk);
            set_type();
        }
    } catch (const tao::pegtl::parse_error& e) {
        m_ui.editor_error_msg = e.what();
        return;
    }
}
