#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Genny.hpp"
#include "Process.hpp"
#include "node/Base.hpp"

class MemoryUi {
public:
    MemoryUi(genny::Sdk& sdk, genny::Struct* struct_, Process& process, uintptr_t address);

    void display();

private:
    genny::Sdk& m_sdk;
    genny::Struct* m_struct{};
    Process& m_process;
    uintptr_t m_address{};

    std::vector<std::byte> m_mem{};
    std::chrono::steady_clock::time_point m_mem_refresh_time{};

    std::unique_ptr<genny::Variable> m_proxy_variable{};
    std::unique_ptr<node::Base> m_root{};

    void refresh_memory();
};