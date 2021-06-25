#include <limits>

#include <Windows.h>

#include <TlHelp32.h>

#include "Windows.hpp"

namespace arch {
WindowsProcess::WindowsProcess(DWORD process_id) : Process{} {
    m_process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, process_id);

    if (m_process == nullptr) {
        return;
    }

    // Iterate modules.
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id);

    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 entry{};

        entry.dwSize = sizeof(entry);

        if (Module32First(snapshot, &entry)) {
            do {
                // Create a module and an allocation for that module since, in testing (win10 x64), the iterate
                // memory step misses the memory allocated for the modules.
                Module m{};
                Allocation a{};

                m.name = entry.szModule;
                m.start = (uintptr_t)entry.modBaseAddr;
                m.size = entry.modBaseSize;
                m.end = m.start + m.size;
                a.start = m.start;
                a.size = m.size;
                a.end = m.end;

                m_modules.emplace_back(std::move(m));
                m_allocations.emplace_back(std::move(a));

            } while (Module32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
    }

    // Iterate memory.
    SYSTEM_INFO si{};
    MEMORY_BASIC_INFORMATION mbi{};

    GetSystemInfo(&si);

    for (auto i = (uintptr_t)si.lpMinimumApplicationAddress; i < (uintptr_t)si.lpMaximumApplicationAddress;
         i = (uintptr_t)mbi.BaseAddress + mbi.RegionSize) {
        VirtualQueryEx(m_process, (LPCVOID)i, &mbi, sizeof(mbi));

        auto protect = mbi.AllocationProtect;
        Allocation a{};

        a.start = (uintptr_t)mbi.BaseAddress;
        a.size = mbi.RegionSize;
        a.end = a.start + a.size;
        a.read = protect & PAGE_READONLY || protect & PAGE_READWRITE || protect & PAGE_EXECUTE_READWRITE;
        a.write = protect & PAGE_READWRITE || protect & PAGE_EXECUTE_READWRITE;
        a.execute = protect & PAGE_EXECUTE_READ || protect & PAGE_EXECUTE_READWRITE;

        if (a.read) {
            m_allocations.emplace_back(std::move(a));
        }
    }
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

std::map<uint32_t, std::string> WindowsHelpers::processes() {
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::map<uint32_t, std::string> pids{};
    PROCESSENTRY32 entry{};

    entry.dwSize = sizeof(entry);

    if (Process32First(snapshot, &entry)) {
        do {
            pids[entry.th32ProcessID] = entry.szExeFile;
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return pids;
}
} // namespace arch
