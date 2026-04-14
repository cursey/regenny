#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>

#include <fmt/format.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sdkgenny.hpp>
#include <spdlog/spdlog.h>

#include "Api.hpp"
#include "ReGenny.hpp"
#include "arch/Arch.hpp"

#ifdef _WIN32
#include "arch/Windows.hpp"
#endif

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Lua output capture sink — captures spdlog output during Lua eval.
// Installed temporarily around each eval call while m_lua_lock is held,
// so there's no concurrency issue with the shared capture buffer.
// ---------------------------------------------------------------------------
#include <spdlog/sinks/base_sink.h>

template <typename Mutex>
class CaptureSink : public spdlog::sinks::base_sink<Mutex> {
public:
    std::string& buffer() { return m_buf; }
    void clear() { m_buf.clear(); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted{};
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        m_buf += fmt::to_string(formatted);
    }
    void flush_() override {}

private:
    std::string m_buf;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static json error_json(const std::string& msg) {
    return json{{"error", msg}};
}

static void json_response(httplib::Response& res, const json& j) {
    res.set_content(j.dump(), "application/json");
}

static void json_error(httplib::Response& res, const std::string& msg, int status = 400) {
    res.status = status;
    json_response(res, error_json(msg));
}

// Parse "0x..." or decimal address string to uintptr_t.
static std::optional<uintptr_t> parse_addr_param(const std::string& s) {
    try {
        size_t pos{};
        auto val = std::stoull(s, &pos, 0);
        if (pos == s.size()) return static_cast<uintptr_t>(val);
    } catch (...) {}
    return std::nullopt;
}

// Recursively serialize a sdkgenny::Struct to JSON (fields, offsets, types, parents).
static json serialize_struct(sdkgenny::Struct* s) {
    json j;
    j["name"] = s->name();
    j["size"] = s->size();

    // Parents
    auto parents = json::array();
    for (auto* parent : s->parents()) {
        parents.push_back(parent->name());
    }
    j["parents"] = parents;

    // Variables (fields)
    auto fields = json::array();
    std::function<void(sdkgenny::Struct*, uintptr_t)> add_vars = [&](sdkgenny::Struct* st, uintptr_t base_offset) {
        for (auto* parent : st->parents()) {
            add_vars(parent, base_offset);
            base_offset += parent->size();
        }
        for (auto* var : st->get_all<sdkgenny::Variable>()) {
            json f;
            f["name"] = var->name();
            f["offset"] = base_offset + var->offset();
            if (var->type()) {
                f["type"] = var->type()->name();
                f["type_size"] = var->type()->size();

                if (var->type()->is_a<sdkgenny::Pointer>()) {
                    f["is_pointer"] = true;
                    auto* ref = dynamic_cast<sdkgenny::Reference*>(var->type());
                    if (ref && ref->to()) {
                        f["points_to"] = ref->to()->name();
                    }
                } else if (var->type()->is_a<sdkgenny::Array>()) {
                    f["is_array"] = true;
                    auto* arr = dynamic_cast<sdkgenny::Array*>(var->type());
                    if (arr) {
                        f["array_count"] = arr->count();
                        if (arr->of()) f["array_element_type"] = arr->of()->name();
                    }
                } else if (var->type()->is_a<sdkgenny::Struct>()) {
                    f["is_struct"] = true;
                } else if (var->type()->is_a<sdkgenny::Enum>()) {
                    f["is_enum"] = true;
                }
            }
            if (var->is_bitfield()) {
                f["is_bitfield"] = true;
                f["bit_offset"] = var->bit_offset();
                f["bit_size"] = var->bit_size();
            }

            // Metadata (vector<string>)
            auto& meta = var->metadata();
            if (!meta.empty()) {
                f["metadata"] = meta;
            }

            fields.push_back(f);
        }
    };
    add_vars(s, 0);
    j["fields"] = fields;

    return j;
}

// ---------------------------------------------------------------------------
// Api
// ---------------------------------------------------------------------------

Api::Api(ReGenny* regenny, int port)
    : m_regenny{regenny}, m_server{std::make_unique<httplib::Server>()} {

    // Determine port: env var > explicit arg > default 12025
    if (auto* env = std::getenv("REGENNY_API_PORT"); env != nullptr) {
        try { m_port = std::stoi(env); } catch (...) { m_port = 12025; }
    } else if (port > 0) {
        m_port = port;
    } else {
        m_port = 12025;
    }

    register_routes();

    m_thread = std::thread(&Api::server_thread_fn, this);
}

Api::~Api() {
    if (m_server) {
        m_server->stop();
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

std::optional<Api::DeferredAttach> Api::consume_deferred_attach() {
    std::scoped_lock lk{m_deferred_lock};
    auto result = std::move(m_deferred_attach);
    m_deferred_attach.reset();
    return result;
}

std::optional<Api::DeferredOpen> Api::consume_deferred_open() {
    std::scoped_lock lk{m_deferred_lock};
    auto result = std::move(m_deferred_open);
    m_deferred_open.reset();
    return result;
}

std::optional<Api::DeferredTypeSelect> Api::consume_deferred_type_select() {
    std::scoped_lock lk{m_deferred_lock};
    auto result = std::move(m_deferred_type_select);
    m_deferred_type_select.reset();
    return result;
}

void Api::server_thread_fn() {
    spdlog::info("[API] Starting HTTP server on port {}...", m_port);
    if (!m_server->listen("127.0.0.1", m_port)) {
        spdlog::error("[API] Failed to start HTTP server on port {}", m_port);
    }
}

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------

void Api::register_routes() {
    auto* rg = m_regenny;

    // Global exception handler — catches any unhandled exception from route handlers
    // (e.g. std::stoull/std::stoi on invalid input) and returns a 500 JSON error
    // instead of crashing the server thread.
    m_server->set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        try {
            if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        } catch (...) {
            res.status = 500;
            res.set_content(json{{"error", "unknown exception"}}.dump(), "application/json");
        }
    });

    // ── Status ───────────────────────────────────────────────────────────
    m_server->Get("/api/status", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        json j;
        auto& proc = rg->process();
        auto& project = rg->project();
        j["attached"] = proc && proc->process_id() != 0;
        j["process_id"] = proc ? proc->process_id() : 0;
        j["process_name"] = project.process_name;
        auto& filepath = rg->open_filepath();
        j["open_file"] = filepath.empty() ? "" : filepath.string();
        if (rg->type()) {
            j["current_type"] = rg->type()->name();
        } else {
            j["current_type"] = nullptr;
        }
        j["current_address"] = fmt::format("0x{:X}", rg->address());
        j["sdk_loaded"] = rg->sdk() != nullptr;
        json_response(res, j);
    });

    // ── Process Management ───────────────────────────────────────────────
    m_server->Get("/api/processes", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        // We need helpers to list processes, but that's private.
        // Use arch::make_helpers() directly.
        auto helpers = arch::make_helpers();
        auto procs = helpers->processes();
        auto arr = json::array();
        for (auto& [pid, name] : procs) {
            arr.push_back(json{{"pid", pid}, {"name", name}});
        }
        json_response(res, arr);
    });

    m_server->Post("/api/attach", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            DeferredAttach da;
            if (body.contains("pid")) da.pid = body["pid"].get<uint32_t>();
            if (body.contains("name")) da.name = body["name"].get<std::string>();

            if (da.pid == 0 && !da.name.empty()) {
                // Resolve name to PID
                auto helpers = arch::make_helpers();
                auto procs = helpers->processes();
                for (auto& [pid, name] : procs) {
                    if (name == da.name) {
                        da.pid = pid;
                        break;
                    }
                }
            }

            if (da.pid == 0) {
                json_error(res, "Could not resolve process. Provide 'pid' or a valid 'name'.");
                return;
            }

            // Fill in name if we only had PID
            if (da.name.empty()) {
                auto helpers = arch::make_helpers();
                auto procs = helpers->processes();
                if (procs.count(da.pid)) da.name = procs[da.pid];
            }

            {
                std::scoped_lock lk{m_deferred_lock};
                m_deferred_attach = da;
            }
            json_response(res, json{{"status", "ok"}, {"pid", da.pid}, {"name", da.name}});
        } catch (const std::exception& e) {
            json_error(res, e.what());
        }
    });

    m_server->Post("/api/detach", [this](const httplib::Request&, httplib::Response& res) {
        m_detach_requested.store(true);
        json_response(res, json{{"status", "ok"}});
    });

    // ── Memory Operations ────────────────────────────────────────────────
    m_server->Get("/api/memory/read", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) {
            json_error(res, "Not attached to a process");
            return;
        }

        auto addr_str = req.get_param_value("address");
        auto size_str = req.get_param_value("size");
        if (addr_str.empty() || size_str.empty()) {
            json_error(res, "Required params: address, size");
            return;
        }

        auto addr = parse_addr_param(addr_str);
        if (!addr) { json_error(res, "Invalid address"); return; }

        auto size = std::min(static_cast<size_t>(std::stoull(size_str, nullptr, 0)), size_t{8192});
        std::vector<uint8_t> buf(size);

        if (!proc->read(*addr, buf.data(), size)) {
            json_error(res, "Read failed");
            return;
        }

        // Format as hex dump with ASCII sidebar
        std::string hex_dump;
        for (size_t i = 0; i < size; i += 16) {
            hex_dump += fmt::format("{:016X}  ", *addr + i);
            std::string ascii;
            for (size_t j = 0; j < 16 && i + j < size; j++) {
                hex_dump += fmt::format("{:02X} ", buf[i + j]);
                ascii += (buf[i + j] >= 0x20 && buf[i + j] < 0x7f) ? static_cast<char>(buf[i + j]) : '.';
            }
            // Pad if last line is short
            for (size_t j = size - i; j < 16; j++) {
                hex_dump += "   ";
            }
            hex_dump += " " + ascii + "\n";
        }

        json j;
        j["address"] = fmt::format("0x{:X}", *addr);
        j["size"] = size;
        j["hex_dump"] = hex_dump;

        // Also provide raw hex bytes
        std::string raw_hex;
        for (size_t i = 0; i < size; i++) {
            raw_hex += fmt::format("{:02X}", buf[i]);
        }
        j["raw_hex"] = raw_hex;

        json_response(res, j);
    });

    m_server->Get("/api/memory/read_typed", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) {
            json_error(res, "Not attached to a process");
            return;
        }

        auto addr_str = req.get_param_value("address");
        auto type_str = req.get_param_value("type");
        auto count_str = req.get_param_value("count");
        if (addr_str.empty() || type_str.empty()) {
            json_error(res, "Required params: address, type. Optional: count");
            return;
        }

        auto addr = parse_addr_param(addr_str);
        if (!addr) { json_error(res, "Invalid address"); return; }

        int count = 1;
        if (!count_str.empty()) count = std::clamp(std::stoi(count_str), 1, 50);

        auto values = json::array();

        for (int i = 0; i < count; i++) {
            auto cur = *addr;

            if (type_str == "u8") {
                auto v = proc->read<uint8_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 1;
            } else if (type_str == "i8") {
                auto v = proc->read<int8_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 1;
            } else if (type_str == "u16") {
                auto v = proc->read<uint16_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 2;
            } else if (type_str == "i16") {
                auto v = proc->read<int16_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 2;
            } else if (type_str == "u32") {
                auto v = proc->read<uint32_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 4;
            } else if (type_str == "i32") {
                auto v = proc->read<int32_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 4;
            } else if (type_str == "u64") {
                auto v = proc->read<uint64_t>(cur); values.push_back(v ? json(fmt::format("0x{:X}", *v)) : json(nullptr)); *addr += 8;
            } else if (type_str == "i64") {
                auto v = proc->read<int64_t>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 8;
            } else if (type_str == "f32") {
                auto v = proc->read<float>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 4;
            } else if (type_str == "f64") {
                auto v = proc->read<double>(cur); values.push_back(v ? json(*v) : json(nullptr)); *addr += 8;
            } else if (type_str == "ptr") {
                auto v = proc->read<uintptr_t>(cur); values.push_back(v ? json(fmt::format("0x{:X}", *v)) : json(nullptr)); *addr += sizeof(uintptr_t);
            } else {
                json_error(res, "Unknown type. Supported: u8,i8,u16,i16,u32,i32,u64,i64,f32,f64,ptr");
                return;
            }
        }

        json j;
        j["address"] = addr_str;
        j["type"] = type_str;
        j["count"] = count;
        j["values"] = values;
        json_response(res, j);
    });

    m_server->Post("/api/memory/write", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) {
            json_error(res, "Not attached to a process");
            return;
        }

        try {
            auto body = json::parse(req.body);
            auto addr_str = body.value("address", "");
            auto type_str = body.value("type", "");

            auto addr = parse_addr_param(addr_str);
            if (!addr) { json_error(res, "Invalid address"); return; }

            bool ok = false;
            if (type_str == "u8") ok = proc->write<uint8_t>(*addr, body["value"].get<uint8_t>());
            else if (type_str == "i8") ok = proc->write<int8_t>(*addr, body["value"].get<int8_t>());
            else if (type_str == "u16") ok = proc->write<uint16_t>(*addr, body["value"].get<uint16_t>());
            else if (type_str == "i16") ok = proc->write<int16_t>(*addr, body["value"].get<int16_t>());
            else if (type_str == "u32") ok = proc->write<uint32_t>(*addr, body["value"].get<uint32_t>());
            else if (type_str == "i32") ok = proc->write<int32_t>(*addr, body["value"].get<int32_t>());
            else if (type_str == "u64") ok = proc->write<uint64_t>(*addr, body["value"].get<uint64_t>());
            else if (type_str == "i64") ok = proc->write<int64_t>(*addr, body["value"].get<int64_t>());
            else if (type_str == "f32") ok = proc->write<float>(*addr, body["value"].get<float>());
            else if (type_str == "f64") ok = proc->write<double>(*addr, body["value"].get<double>());
            else { json_error(res, "Unknown type"); return; }

            json_response(res, json{{"status", ok ? "ok" : "write_failed"}});
        } catch (const std::exception& e) {
            json_error(res, e.what());
        }
    });

    m_server->Get("/api/memory/read_string", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) {
            json_error(res, "Not attached to a process");
            return;
        }

        auto addr_str = req.get_param_value("address");
        auto max_len_str = req.get_param_value("max_length");
        if (addr_str.empty()) { json_error(res, "Required param: address"); return; }

        auto addr = parse_addr_param(addr_str);
        if (!addr) { json_error(res, "Invalid address"); return; }

        size_t max_len = 256;
        if (!max_len_str.empty()) max_len = std::clamp<size_t>(std::stoull(max_len_str, nullptr, 0), 1, 4096);

        std::string result;
        for (size_t i = 0; i < max_len; i++) {
            auto byte = proc->read<uint8_t>(*addr + i);
            if (!byte || *byte == 0) break;
            result += static_cast<char>(*byte);
        }

        json_response(res, json{{"address", addr_str}, {"value", result}, {"length", result.size()}});
    });

    m_server->Get("/api/modules", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) {
            json_error(res, "Not attached to a process");
            return;
        }

        auto arr = json::array();
        for (auto& mod : proc->modules()) {
            arr.push_back(json{
                {"name", mod.name},
                {"start", fmt::format("0x{:X}", mod.start)},
                {"end", fmt::format("0x{:X}", mod.end)},
                {"size", mod.size}
            });
        }
        json_response(res, arr);
    });

    m_server->Get("/api/allocations", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) {
            json_error(res, "Not attached to a process");
            return;
        }

        auto arr = json::array();
        for (auto& alloc : proc->allocations()) {
            arr.push_back(json{
                {"start", fmt::format("0x{:X}", alloc.start)},
                {"end", fmt::format("0x{:X}", alloc.end)},
                {"size", alloc.size},
                {"read", alloc.read},
                {"write", alloc.write},
                {"execute", alloc.execute}
            });
        }
        json_response(res, arr);
    });

    // ── Genny File Operations ────────────────────────────────────────────
    m_server->Get("/api/genny/content", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& filepath = rg->open_filepath();
        if (filepath.empty()) {
            json_error(res, "No file open");
            return;
        }

        try {
            std::ifstream f{filepath};
            if (!f.is_open()) {
                json_error(res, "Failed to open file", 500);
                return;
            }
            std::string content{std::istreambuf_iterator<char>(f), {}};
            json_response(res, json{{"path", filepath.string()}, {"content", content}});
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    m_server->Post("/api/genny/content", [this, rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& filepath = rg->open_filepath();
        if (filepath.empty()) {
            json_error(res, "No file open");
            return;
        }

        try {
            auto body = json::parse(req.body);
            auto content = body.value("content", "");

            std::ofstream f{filepath};
            f << content;
            f.close();

            // Request re-parse on main thread
            m_reparse_requested.store(true);

            json_response(res, json{{"status", "ok"}, {"path", filepath.string()}});
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    m_server->Get("/api/genny/path", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& filepath = rg->open_filepath();
        json_response(res, json{{"path", filepath.empty() ? "" : filepath.string()}});
    });

    m_server->Post("/api/genny/open", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto path = body.value("path", "");
            if (path.empty()) { json_error(res, "Required: path"); return; }
            if (!std::filesystem::exists(path)) { json_error(res, "File not found: " + path); return; }

            {
                std::scoped_lock lk{m_deferred_lock};
                m_deferred_open = DeferredOpen{path};
            }
            json_response(res, json{{"status", "ok"}, {"path", path}});
        } catch (const std::exception& e) {
            json_error(res, e.what());
        }
    });

    m_server->Post("/api/genny/new", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto path = body.value("path", "");
            auto content = body.value("content", "");

            if (path.empty()) { json_error(res, "Required: path"); return; }

            // Ensure .genny extension
            std::filesystem::path fpath{path};
            if (fpath.extension() != ".genny") {
                fpath.replace_extension(".genny");
            }

            // Provide default content if none given
            if (content.empty()) {
                content = "type int 4 [[i32]]\n\nstruct MyStruct {\n    int value @ 0\n}\n";
            }

            std::ofstream f{fpath};
            f << content;
            f.close();

            {
                std::scoped_lock lk{m_deferred_lock};
                m_deferred_open = DeferredOpen{fpath.string()};
            }
            json_response(res, json{{"status", "ok"}, {"path", fpath.string()}});
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    m_server->Post("/api/genny/reload", [this](const httplib::Request&, httplib::Response& res) {
        m_reparse_requested.store(true);
        json_response(res, json{{"status", "ok"}});
    });

    // ── Type Introspection ───────────────────────────────────────────────
    m_server->Get("/api/types", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& sdk = rg->sdk();
        if (!sdk) { json_error(res, "No SDK loaded (open a .genny file first)"); return; }

        std::unordered_set<sdkgenny::Struct*> structs{};
        sdk->global_ns()->get_all_in_children<sdkgenny::Struct>(structs);

        auto arr = json::array();
        for (auto* s : structs) {
            // Build fully qualified name
            std::vector<std::string> parts;
            for (auto* p = s->owner<sdkgenny::Object>(); p && !p->is_a<sdkgenny::Sdk>(); p = p->owner<sdkgenny::Object>()) {
                if (!p->name().empty()) parts.push_back(p->name());
            }
            std::reverse(parts.begin(), parts.end());
            std::string fqn;
            for (auto& part : parts) fqn += part + ".";
            fqn += s->name();

            arr.push_back(json{{"name", fqn}, {"size", s->size()}});
        }

        json_response(res, arr);
    });

    m_server->Get("/api/type", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& sdk = rg->sdk();
        if (!sdk) { json_error(res, "No SDK loaded"); return; }

        auto name = req.get_param_value("name");
        if (name.empty()) { json_error(res, "Required param: name"); return; }

        // Find the struct by name (support dot-separated and plain names)
        std::unordered_set<sdkgenny::Struct*> structs{};
        sdk->global_ns()->get_all_in_children<sdkgenny::Struct>(structs);

        sdkgenny::Struct* found = nullptr;
        for (auto* s : structs) {
            // Build FQN
            std::vector<std::string> parts;
            for (auto* p = s->owner<sdkgenny::Object>(); p && !p->is_a<sdkgenny::Sdk>(); p = p->owner<sdkgenny::Object>()) {
                if (!p->name().empty()) parts.push_back(p->name());
            }
            std::reverse(parts.begin(), parts.end());
            std::string fqn;
            for (auto& part : parts) fqn += part + ".";
            fqn += s->name();

            if (fqn == name || s->name() == name) {
                found = s;
                break;
            }
        }

        if (!found) { json_error(res, "Type not found: " + name, 404); return; }

        json_response(res, serialize_struct(found));
    });

    m_server->Get("/api/type/address", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto name = req.get_param_value("name");
        if (name.empty()) { json_error(res, "Required param: name"); return; }

        auto& project = rg->project();
        auto it = project.type_addresses.find(name);
        if (it != project.type_addresses.end()) {
            json_response(res, json{{"name", name}, {"address", it->second}});
        } else {
            json_response(res, json{{"name", name}, {"address", nullptr}});
        }
    });

    m_server->Post("/api/type/select", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto name = body.value("name", "");
            auto address = body.value("address", "");
            if (name.empty()) { json_error(res, "Required: name"); return; }

            {
                std::scoped_lock lk{m_deferred_lock};
                m_deferred_type_select = DeferredTypeSelect{name, address};
            }
            json_response(res, json{{"status", "ok"}, {"name", name}, {"address", address}});
        } catch (const std::exception& e) {
            json_error(res, e.what());
        }
    });

    // ── Lua Scripting ────────────────────────────────────────────────────
    m_server->Post("/api/lua/eval", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        try {
            auto body = json::parse(req.body);
            auto code = body.value("code", "");
            if (code.empty()) { json_error(res, "Required: code"); return; }

            // Create a per-request logger with two sinks:
            //   1. CaptureSink — raw "%v" output for the API JSON response
            //   2. UI logger sink — so the user sees output in the ReGenny Log panel
            auto capture = std::make_shared<CaptureSink<std::mutex>>();
            auto ui_sink = rg->logger().logger()->sinks().front();
            auto eval_logger = std::make_shared<spdlog::logger>("api_eval",
                spdlog::sinks_init_list{capture, ui_sink});
            capture->set_pattern("%v");

            std::string result_str;
            bool success = true;
            std::string error_msg;

            {
                auto& lua_lock = rg->lua_lock();
                std::scoped_lock lk{lua_lock};
                auto& lua = rg->lua();

                auto prev_logger = spdlog::default_logger();
                spdlog::set_default_logger(eval_logger);

                // Try as expression first
                auto result = lua.safe_script(std::string{"return "} + code, sol::script_pass_on_error);

                if (result.valid()) {
                    auto obj = result.get<sol::object>();
                    if (obj.get_type() != sol::type::none && obj.get_type() != sol::type::lua_nil) {
                        obj.push();
                        auto str = luaL_tolstring(lua, -1, nullptr);
                        if (str) result_str = str;
                        lua_pop(lua, 2);
                    }
                } else {
                    // Expression failed, try as statement
                    auto stmt_result = lua.safe_script(code, sol::script_pass_on_error);
                    if (!stmt_result.valid()) {
                        success = false;
                        sol::error err = stmt_result;
                        error_msg = err.what();
                    }
                }

                spdlog::set_default_logger(prev_logger);
            }

            json j;
            j["success"] = success;
            j["output"] = capture->buffer();
            if (!result_str.empty()) j["result"] = result_str;
            if (!error_msg.empty()) j["error"] = error_msg;

            json_response(res, j);
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    m_server->Post("/api/lua/exec_file", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        try {
            auto body = json::parse(req.body);
            auto path = body.value("path", "");
            if (path.empty()) { json_error(res, "Required: path"); return; }
            if (!std::filesystem::exists(path)) { json_error(res, "File not found: " + path); return; }

            auto capture = std::make_shared<CaptureSink<std::mutex>>();
            auto ui_sink = rg->logger().logger()->sinks().front();
            auto eval_logger = std::make_shared<spdlog::logger>("api_exec",
                spdlog::sinks_init_list{capture, ui_sink});
            capture->set_pattern("%v");

            bool success = true;
            std::string error_msg;

            {
                auto& lua_lock = rg->lua_lock();
                std::scoped_lock lk{lua_lock};
                auto& lua = rg->lua();

                auto prev_logger = spdlog::default_logger();
                spdlog::set_default_logger(eval_logger);

                auto result = lua.safe_script_file(path, sol::script_pass_on_error);
                if (!result.valid()) {
                    success = false;
                    sol::error err = result;
                    error_msg = err.what();
                }

                spdlog::set_default_logger(prev_logger);
            }

            json j;
            j["success"] = success;
            j["output"] = capture->buffer();
            if (!error_msg.empty()) j["error"] = error_msg;
            json_response(res, j);
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    m_server->Post("/api/lua/reset", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& lua_lock = rg->lua_lock();
        std::scoped_lock lk{lua_lock};
        rg->reset_lua_state_api();
        json_response(res, json{{"status", "ok"}});
    });

    m_server->Get("/api/lua/script", [](const httplib::Request& req, httplib::Response& res) {
        auto path = req.get_param_value("path");
        if (path.empty()) { json_error(res, "Required param: path"); return; }
        if (!std::filesystem::exists(path)) { json_error(res, "File not found: " + path, 404); return; }

        try {
            std::ifstream f{path};
            if (!f.is_open()) {
                json_error(res, "Failed to open file", 500);
                return;
            }
            std::string content{std::istreambuf_iterator<char>(f), {}};
            json_response(res, json{{"path", path}, {"content", content}});
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    m_server->Post("/api/lua/script", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto path = body.value("path", "");
            auto content = body.value("content", "");
            if (path.empty()) { json_error(res, "Required: path"); return; }

            // Ensure directory exists
            auto dir = std::filesystem::path{path}.parent_path();
            if (!dir.empty()) std::filesystem::create_directories(dir);

            std::ofstream f{path};
            f << content;
            f.close();

            json_response(res, json{{"status", "ok"}, {"path", path}});
        } catch (const std::exception& e) {
            json_error(res, e.what(), 500);
        }
    });

    // ── Project ──────────────────────────────────────────────────────────
    m_server->Get("/api/project", [rg](const httplib::Request&, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& project = rg->project();
        json j;
        j["process_name"] = project.process_name;
        j["process_id"] = project.process_id;
        j["type_chosen"] = project.type_chosen;
        j["type_addresses"] = project.type_addresses;
        j["extension_header"] = project.extension_header;
        j["extension_source"] = project.extension_source;

        auto tabs = json::array();
        for (auto& tab : project.tabs) {
            tabs.push_back(json{{"name", tab.name}, {"type_name", tab.type_name}, {"address", tab.address}});
        }
        j["tabs"] = tabs;
        j["active_tab_index"] = project.active_tab_index;

        json_response(res, j);
    });

    // ── RTTI ─────────────────────────────────────────────────────────────
#ifdef _WIN32
    m_server->Get("/api/rtti/typename", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) { json_error(res, "Not attached"); return; }

        auto addr_str = req.get_param_value("address");
        auto addr = parse_addr_param(addr_str);
        if (!addr) { json_error(res, "Invalid address"); return; }

        auto* wp = dynamic_cast<arch::WindowsProcess*>(proc.get());
        if (!wp) { json_error(res, "RTTI only available on Windows processes"); return; }

        auto name = wp->get_typename(*addr);
        if (name) {
            json_response(res, json{{"address", addr_str}, {"typename", *name}});
        } else {
            json_response(res, json{{"address", addr_str}, {"typename", nullptr}});
        }
    });

    m_server->Get("/api/rtti/vtable_typename", [rg](const httplib::Request& req, httplib::Response& res) {
        std::shared_lock state_lk{rg->state_mtx()};
        auto& proc = rg->process();
        if (!proc || proc->process_id() == 0) { json_error(res, "Not attached"); return; }

        auto addr_str = req.get_param_value("address");
        auto addr = parse_addr_param(addr_str);
        if (!addr) { json_error(res, "Invalid address"); return; }

        auto* wp = dynamic_cast<arch::WindowsProcess*>(proc.get());
        if (!wp) { json_error(res, "RTTI only available on Windows processes"); return; }

        auto name = wp->get_typename_from_vtable(*addr);
        if (name) {
            json_response(res, json{{"address", addr_str}, {"typename", *name}});
        } else {
            json_response(res, json{{"address", addr_str}, {"typename", nullptr}});
        }
    });
#endif

    // ── Help ─────────────────────────────────────────────────────────────
    m_server->Get("/api/help", [](const httplib::Request&, httplib::Response& res) {
        // Try to find AGENT.md relative to the executable
        auto exe_dir = std::filesystem::current_path();

        for (auto& candidate : {
            exe_dir / "AGENT.md",
            exe_dir / ".." / "AGENT.md",
            exe_dir / "mcp-server" / "AGENT.md",
        }) {
            if (std::filesystem::exists(candidate)) {
                std::ifstream f{candidate};
                if (!f.is_open()) {
                    continue;
                }
                std::string content{std::istreambuf_iterator<char>(f), {}};
                res.set_content(content, "text/markdown");
                return;
            }
        }

        json_error(res, "AGENT.md not found", 404);
    });
}
