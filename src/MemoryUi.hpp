#pragma once

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Genny.hpp>

#include "Config.hpp"
#include "Process.hpp"
#include "node/Base.hpp"
#include "node/Property.hpp"

class MemoryUi {
public:
    MemoryUi(Config& cfg, genny::Sdk& sdk, genny::Struct* struct_, Process& process, node::Property& inherited_props);

    void display(uintptr_t address);

    auto&& props() { return m_props; }

private:
    Config& m_cfg;
    genny::Sdk& m_sdk;
    genny::Struct* m_struct{};
    Process& m_process;

    std::unique_ptr<genny::Variable> m_proxy_variable{};
    std::unique_ptr<node::Base> m_root{};

    node::Property m_props;

    std::string m_header{};
};