#include "LoggerUi.hpp"

void LoggerUi::ui() {
    ImGui::BeginChild("logger");
    ImGui::TextUnformatted(m_buf.begin());

    if (m_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }

    ImGui::EndChild();
}
