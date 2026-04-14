#include "LoggerUi.hpp"

void LoggerUi::ui() {
    // Drain queued messages from background threads into the ImGui buffer.
    // This is the only place m_buf is mutated, and it runs on the main thread,
    // so ImGui::TextUnformatted below always reads a stable buffer.
    m_sink->flush_to(m_buf, m_scroll_to_bottom);

    ImGui::BeginChild("logger");
    ImGui::TextUnformatted(m_buf.begin());

    if (m_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }

    ImGui::EndChild();
}
