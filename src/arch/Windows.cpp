#include <Windows.h>

#include <TlHelp32.h>

#include "Windows.hpp"

namespace arch {
WindowsModule::WindowsModule(std::string name, uintptr_t address, size_t size) : 
    m_name{std::move(name)}, m_address{address}, m_size{size} {
}

WindowsProcess::WindowsProcess(DWORD process_id) {
    m_process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, process_id);
}

bool WindowsProcess::read(uintptr_t address, void* buffer, size_t size) {
    SIZE_T bytes_read{};

    ReadProcessMemory(m_process, (LPCVOID)address, buffer, size, &bytes_read);

    return bytes_read == size;
}

bool WindowsProcess::write(uintptr_t address, const void* buffer, size_t size) {
    SIZE_T bytes_written{};

    WriteProcessMemory(m_process, (LPVOID)address, buffer, size, &bytes_written);

    return bytes_written == size;
}

uint32_t WindowsProcess::process_id() {
    return GetProcessId(m_process);
}

std::vector<std::unique_ptr<Module>> WindowsProcess::modules() {
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id());

    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::vector<std::unique_ptr<Module>> modules{};
    MODULEENTRY32 entry{};

    entry.dwSize = sizeof(entry);

    if (Module32First(snapshot, &entry)) {
        do {
            modules.push_back(
                std::make_unique<WindowsModule>(entry.szModule, (uintptr_t)entry.modBaseAddr, entry.modBaseSize));
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return modules;
}

std::map<std::string, uint32_t> WindowsHelpers::processes() {
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::map<std::string, uint32_t> pids{};
    PROCESSENTRY32 entry{};

    entry.dwSize = sizeof(entry);

    if (Process32First(snapshot, &entry)) {
        do {
            pids[entry.szExeFile] = entry.th32ProcessID;
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return pids;
}
} // namespace arch
