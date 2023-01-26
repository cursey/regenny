#pragma once

#include "Helpers.hpp"
#include "Process.hpp"

namespace arch {
class MacProcess : public Process {
public:
    MacProcess(uint32_t process_id) : Process{} {}
};

class MacHelpers : public Helpers {
    std::map<uint32_t, std::string> processes() override { return {}; }
};
} // namespace arch