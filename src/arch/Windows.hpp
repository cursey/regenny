#pragma once

#include <Windows.h>

#include "Helpers.hpp"
#include "Process.hpp"

namespace arch {
class WindowsProcess : public Process {
public:
    WindowsProcess(DWORD process_id);

    bool read(uintptr_t address, void* buffer, size_t size) override;
    bool write(uintptr_t address, const void* buffer, size_t size) override;
    uint32_t process_id() override;
    bool ok() override { return m_process != nullptr; }

private:
    HANDLE m_process{};
};

class WindowsHelpers : public Helpers {
public:
    std::map<uint32_t, std::string> processes() override;
};
} // namespace arch