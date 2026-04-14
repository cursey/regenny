#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class ReGenny;

namespace httplib {
class Server;
}

// Embedded HTTP API server for MCP integration.
// Runs on a background thread, provides JSON endpoints for all ReGenny operations.
class Api {
public:
    explicit Api(ReGenny* regenny, int port = 0);
    ~Api();

    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;

    int port() const { return m_port; }

    // Deferred operations — checked by ReGenny::update() on the main thread.
    // These exist because PEGTL parsing and UI state mutations must happen on the main thread.
    bool should_reparse() { return m_reparse_requested.exchange(false); }
    bool should_detach() { return m_detach_requested.exchange(false); }

    struct DeferredAttach {
        uint32_t pid{};
        std::string name{};
    };
    // Returns nullopt if no attach is pending.
    std::optional<DeferredAttach> consume_deferred_attach();

    struct DeferredOpen {
        std::string filepath{};
    };
    std::optional<DeferredOpen> consume_deferred_open();

    struct DeferredTypeSelect {
        std::string type_name{};
        std::string address{};
    };
    std::optional<DeferredTypeSelect> consume_deferred_type_select();

private:
    void register_routes();
    void server_thread_fn();

    ReGenny* m_regenny{};
    int m_port{};
    std::unique_ptr<httplib::Server> m_server;
    std::thread m_thread;

    // Deferred operation state — written by HTTP thread, consumed by main thread.
    std::atomic<bool> m_reparse_requested{false};
    std::atomic<bool> m_detach_requested{false};

    std::mutex m_deferred_lock;
    std::optional<DeferredAttach> m_deferred_attach;
    std::optional<DeferredOpen> m_deferred_open;
    std::optional<DeferredTypeSelect> m_deferred_type_select;
};
