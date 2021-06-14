#include <fmt/format.h>
#include <imgui.h>

#include "Undefined.hpp"

namespace node {
Undefined::Undefined(Process& process, size_t size) : Base{process}, m_size{size} {
}

void Undefined::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    display_address_offset(address, offset);
    ImGui::SameLine();

    static std::string s{};

    s.clear();

    for (auto i = 0; i < m_size; ++i) {
        fmt::format_to(std::back_inserter(s), "{:02X}", *(uint8_t*)&mem[i]);
    }

    ImGui::TextUnformatted(s.c_str());
}

size_t Undefined::size() {
    return m_size;
}
} // namespace node