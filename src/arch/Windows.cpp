#include <limits>

#include <sstream>

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

                m.name = entry.szModule;
                m.start = (uintptr_t)entry.modBaseAddr;
                m.size = entry.modBaseSize;
                m.end = m.start + m.size;

                m_modules.emplace_back(std::move(m));

            } while (Module32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
    }

    auto record_allocation = [this](uintptr_t address) {
        // Skip over allocations we already know about.
        for (auto&& allocation : m_allocations) {
            if (allocation.start == address) {
                return allocation.end;
            }
        }

        MEMORY_BASIC_INFORMATION mbi{};

        VirtualQueryEx(m_process, (LPCVOID)address, &mbi, sizeof(mbi));

        auto protect = mbi.Protect;
        Allocation a{};

        a.start = (uintptr_t)mbi.BaseAddress;
        a.size = mbi.RegionSize;
        a.end = a.start + a.size;
        a.read = protect & PAGE_READONLY || protect & PAGE_READWRITE || protect & PAGE_WRITECOPY ||
                 protect & PAGE_EXECUTE_READ || protect & PAGE_EXECUTE_READWRITE || protect & PAGE_EXECUTE_WRITECOPY;
        a.write = protect & PAGE_READWRITE || protect & PAGE_WRITECOPY || protect & PAGE_EXECUTE_READWRITE ||
                  protect & PAGE_EXECUTE_WRITECOPY;
        a.execute = protect & PAGE_EXECUTE_READ || protect & PAGE_EXECUTE_READWRITE || protect & PAGE_EXECUTE_WRITECOPY;

        if (a.read) {
            // We cache read-only memory allocations in their entirety because Process::read optimizes for read-only
            // reads.
            if (!a.write) {
                ReadOnlyAllocation ro{};
                ro.start = a.start;
                ro.size = a.size;
                ro.end = a.end;
                ro.read = a.read;
                ro.write = a.write;
                ro.execute = a.execute;
                ro.mem.resize(ro.size);

                if (read(ro.start, ro.mem.data(), ro.size)) {
                    m_read_only_allocations.emplace_back(std::move(ro));
                }
            }

            m_allocations.emplace_back(std::move(a));
        }

        return (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    };

    // Iterate module memory.
    for (auto&& mod : m_modules) {
        for (auto i = mod.start; i < mod.end;) {
            i = record_allocation(i);
        }
    }

    // Iterate memory.
    SYSTEM_INFO si{};

    GetSystemInfo(&si);

    for (auto i = (uintptr_t)si.lpMinimumApplicationAddress; i < (uintptr_t)si.lpMaximumApplicationAddress;) {
        i = record_allocation(i);
    }
}

bool WindowsProcess::write(uintptr_t address, const void* buffer, size_t size) {
    SIZE_T bytes_written{};

    WriteProcessMemory(m_process, (LPVOID)address, buffer, size, &bytes_written);

    return bytes_written == size;
}

uint32_t WindowsProcess::process_id() {
    return GetProcessId(m_process);
}

bool WindowsProcess::handle_read(uintptr_t address, void* buffer, size_t size) {
    SIZE_T bytes_read{};

    ReadProcessMemory(m_process, (LPCVOID)address, buffer, size, &bytes_read);

    return bytes_read == size;
}

std::optional<uintptr_t> WindowsProcess::get_complete_object_locator_ptr(uintptr_t ptr) {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto vtable = Process::read<uintptr_t>(ptr);

    if (!vtable || *vtable == 0) {
        return std::nullopt;
    }

    return Process::read<uintptr_t>(*vtable - sizeof(void*));
}

std::optional<_s_RTTICompleteObjectLocator> WindowsProcess::get_complete_object_locator(uintptr_t ptr) {
    auto out_ptr = get_complete_object_locator_ptr(ptr);

    if (!out_ptr) {
        return std::nullopt;
    }

    return Process::read<_s_RTTICompleteObjectLocator>(*out_ptr);
}

std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> WindowsProcess::get_typeinfo(uintptr_t ptr) {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto locator_ptr = get_complete_object_locator_ptr(ptr);

    if (!locator_ptr || *locator_ptr == 0) {
        return std::nullopt;
    }

    auto locator = Process::read<_s_RTTICompleteObjectLocator>(*locator_ptr);

    if (!locator) {
        return std::nullopt;
    }

    auto type_desc_pre = locator->pTypeDescriptor;

    // x64 usually
#if _RTTI_RELATIVE_TYPEINFO
    if (type_desc_pre == 0) {
        return std::nullopt;
    }

    uintptr_t image_base = 0;

    if (locator->signature == COL_SIG_REV0) {
        auto module_within = get_module_within(*locator_ptr);
        image_base = module_within->start;
    }
    else {
        image_base = *locator_ptr - locator->pSelf;
    }

    auto ti = image_base + type_desc_pre;

    return Process::read<std::array<uint8_t, sizeof(std::type_info) + 256>>((uintptr_t)ti);
#else
    if (type_desc_pre == nullptr) {
        return std::nullopt;
    }

    return Process::read<std::array<uint8_t, sizeof(std::type_info) + 256>>((uintptr_t)type_desc_pre);
#endif
}

std::optional<std::string> WindowsProcess::get_typename(uintptr_t ptr) {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto typeinfo = get_typeinfo(ptr);

    if (!typeinfo) {
        return std::nullopt;
    }

    auto ti = (std::type_info*)&*typeinfo;

    return ti->raw_name();
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
