#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

// Thread-safe sink that queues formatted messages for main-thread consumption.
// sink_it_ can be called from any thread; the queued messages are flushed
// into the ImGuiTextBuffer by LoggerUi::ui() on the main thread.
template <typename Mutex> class LoggerUiSink : public spdlog::sinks::base_sink<Mutex> {
public:
    // Drain all queued messages into the target buffer. Call from main thread only.
    void flush_to(ImGuiTextBuffer& buf, bool& scroll_to_bottom) {
        std::scoped_lock lk{m_queue_lock};
        if (m_queue.empty()) return;
        for (auto& msg : m_queue) {
            buf.append(msg.c_str());
        }
        m_queue.clear();
        scroll_to_bottom = true;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted{};
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

        std::scoped_lock lk{m_queue_lock};
        m_queue.push_back(fmt::to_string(formatted));
    }

    void flush_() override {}

private:
    std::mutex m_queue_lock;
    std::vector<std::string> m_queue;
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
        std::make_shared<LoggerUiSink<std::mutex>>()};
    std::shared_ptr<spdlog::logger> m_logger{std::make_shared<spdlog::logger>("LoggerUi", m_sink)};
};
