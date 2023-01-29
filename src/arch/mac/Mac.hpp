#pragma once

#include <mach/mach.h>

#include "Helpers.hpp"
#include "Process.hpp"

namespace arch {
class MacProcess : public Process {
public:
    MacProcess(uint32_t process_id);

    bool ok() override { return m_process != 0; }

protected:
    bool handle_read(uintptr_t address, void* buffer, size_t size) override;

private:
    task_t m_process{};
};

class MacHelpers : public Helpers {
    std::map<uint32_t, std::string> processes() override;
};
} // namespace arch