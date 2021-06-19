#pragma once

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Genny.hpp"
#include "Process.hpp"
#include "node/Base.hpp"
#include "node/Property.hpp"

class MemoryUi {
public:
    MemoryUi(genny::Sdk& sdk, genny::Struct* struct_, Process& process, uintptr_t address, node::Property& inherited_props);

    void display();

    auto&& props() { return m_props; }

private:
    genny::Sdk& m_sdk;
    genny::Struct* m_struct{};
    Process& m_process;
    uintptr_t m_address{};

    std::unique_ptr<genny::Variable> m_proxy_variable{};
    std::unique_ptr<node::Base> m_root{};

    node::Property m_props;
};