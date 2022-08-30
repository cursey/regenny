#pragma once

#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

template <typename Mutex> class LoggerUiSink : public spdlog::sinks::base_sink<Mutex> {
public:
    LoggerUiSink(ImGuiTextBuffer& buf, bool& scroll_to_bottom) : m_buf{buf}, m_scroll_to_bottom{scroll_to_bottom} {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted{};
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        m_buf.append(fmt::to_string(formatted).c_str());
        m_scroll_to_bottom = true;
    }

    void flush_() override {}

private:
    ImGuiTextBuffer& m_buf{};
    bool& m_scroll_to_bottom{};
};

class LoggerUi {
public:
    void ui();

    auto&& logger() const { return m_logger; }
    auto&& logger() { return m_logger; }

    void clear() { m_buf.clear(); }

private:
    ImGuiTextBuffer m_buf{};
    bool m_scroll_to_bottom{};
    std::shared_ptr<LoggerUiSink<std::mutex>> m_sink{
        std::make_shared<LoggerUiSink<std::mutex>>(m_buf, m_scroll_to_bottom)};
    std::shared_ptr<spdlog::logger> m_logger{std::make_shared<spdlog::logger>("LoggerUi", m_sink)};
};