#pragma once

#include <Windows.h>

#include "Helpers.hpp"
#include "Module.hpp"
#include "Process.hpp"

namespace arch {
class WindowsModule : public Module {
public:
    WindowsModule(std::string name, uintptr_t address, size_t size);

    const char* name() override { return m_name.c_str(); }
    uintptr_t address() override { return m_address; }
    size_t size() override { return m_size; }

private:
    std::string m_name{};
    uintptr_t m_address{};
    size_t m_size{};
};

class WindowsProcess : public Process {
public:
    WindowsProcess(DWORD process_id);

    bool read(uintptr_t address, void* buffer, size_t size) override;
    bool write(uintptr_t address, const void* buffer, size_t size) override;
    uint32_t process_id() override;
    std::vector<std::unique_ptr<Module>> modules() override;
    bool ok() override { return m_process != nullptr; }

private:
    HANDLE m_process{};
};

class WindowsHelpers : public Helpers {
public:
    std::map<uint32_t, std::string> processes() override;
};
} // namespace arch