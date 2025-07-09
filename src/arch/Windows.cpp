#include <limits>

#include <sstream>

#include <Windows.h>

#include <TlHelp32.h>

#include "Windows.hpp"

namespace arch {
WindowsProcess::WindowsProcess(DWORD process_id) : Process{} {
    m_process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, process_id);

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
                Module m{};

                m.name = entry.szExePath;
                m.start = (uintptr_t)entry.modBaseAddr;
                m.size = entry.modBaseSize;
                m.end = m.start + m.size;

                m_modules.emplace_back(std::move(m));

            } while (Module32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
    }

    // Iterate memory.
    uintptr_t address = 0;
    MEMORY_BASIC_INFORMATION mbi{};

    while (VirtualQueryEx(m_process, (LPCVOID)address, &mbi, sizeof(mbi)) != 0) {
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

        address += mbi.RegionSize;
    }
}

uint32_t WindowsProcess::process_id() {
    return GetProcessId(m_process);
}

bool WindowsProcess::ok() {
    if (m_process == nullptr) {
        return false;
    }

    DWORD exitcode{};

    GetExitCodeProcess(m_process, &exitcode);

    return exitcode == STILL_ACTIVE;
}

bool WindowsProcess::handle_write(uintptr_t address, const void* buffer, size_t size) {
    SIZE_T bytes_written{};

    WriteProcessMemory(m_process, (LPVOID)address, buffer, size, &bytes_written);

    return bytes_written == size;
}

bool WindowsProcess::handle_read(uintptr_t address, void* buffer, size_t size) {
    SIZE_T bytes_read{};

    if (ReadProcessMemory(m_process, (LPCVOID)address, buffer, size, &bytes_read) == 0) {
        // Query the page to get the real size, and re-read.
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQueryEx(m_process, (LPCVOID)address, &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        size = ((uintptr_t)mbi.BaseAddress + mbi.RegionSize) - address;

        if (ReadProcessMemory(m_process, (LPCVOID)address, buffer, size, &bytes_read) == 0) {
            return false;
        }
    }

    return bytes_read == size;
}

std::optional<uintptr_t> WindowsProcess::handle_allocate(uintptr_t address, size_t size, uint64_t flags) {
    if (auto ptr = VirtualAllocEx(m_process, (LPVOID)address, size, MEM_COMMIT, (DWORD)flags); ptr != nullptr) {
        return (uintptr_t)ptr;
    }

    return std::nullopt;
}

std::optional<uint64_t> WindowsProcess::handle_protect(uintptr_t address, size_t size, uint64_t flags) {
    DWORD old_protect{};

    if (VirtualProtectEx(m_process, (LPVOID)address, size, (DWORD)flags, &old_protect) != 0) {
        return (uint64_t)old_protect;
    }

    return std::nullopt;
}

std::optional<uintptr_t> WindowsProcess::get_complete_object_locator_ptr_from_vtable(uintptr_t vtable) {
    if (vtable == 0) {
        return std::nullopt;
    }

    return Process::read<uintptr_t>(vtable - sizeof(void*));
}

std::optional<uintptr_t> WindowsProcess::get_complete_object_locator_ptr(uintptr_t ptr) {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto vtable = Process::read<uintptr_t>(ptr);

    if (!vtable || *vtable == 0) {
        return std::nullopt;
    }

    return get_complete_object_locator_ptr_from_vtable(*vtable);
}

std::optional<_s_RTTICompleteObjectLocator> WindowsProcess::get_complete_object_locator(uintptr_t ptr) {
    auto out_ptr = get_complete_object_locator_ptr(ptr);

    if (!out_ptr) {
        return std::nullopt;
    }

    return Process::read<_s_RTTICompleteObjectLocator>(*out_ptr);
}

std::optional<uintptr_t> WindowsProcess::resolve_object_base_address(uintptr_t ptr) {
    auto locator = get_complete_object_locator(ptr);

    if (!locator) {
        return std::nullopt;
    }

    return ptr - locator->offset;
}

std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> WindowsProcess::try_get_typeinfo_from_locator(uintptr_t locator_ptr) {
    auto locator = Process::read<_s_RTTICompleteObjectLocator>(locator_ptr);

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
        auto module_within = get_module_within(locator_ptr);

        if (!module_within) {
            return std::nullopt;
        }

        image_base = module_within->start;
    } else {
        image_base = locator_ptr - locator->pSelf;
    }

    auto ti = image_base + type_desc_pre;

    return Process::read<std::array<uint8_t, sizeof(std::type_info) + 256>>((uintptr_t)ti);
#else
    if (type_desc_pre == nullptr || get_module_within((uintptr_t)type_desc_pre) == nullptr) {
        return std::nullopt;
    }

    return Process::read<std::array<uint8_t, sizeof(std::type_info) + 256>>((uintptr_t)type_desc_pre);
#endif
}

std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> WindowsProcess::try_get_typeinfo_from_ptr(uintptr_t ptr) {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto locator_ptr = get_complete_object_locator_ptr(ptr);

    if (!locator_ptr || *locator_ptr == 0) {
        return std::nullopt;
    }

    return try_get_typeinfo_from_locator(*locator_ptr);
}

std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> WindowsProcess::try_get_typeinfo_from_vtable(uintptr_t vtable) {
    if (vtable == 0) {
        return std::nullopt;
    }

    auto locator_ptr = get_complete_object_locator_ptr_from_vtable(vtable);

    if (!locator_ptr || *locator_ptr == 0) {
        return std::nullopt;
    }

    return try_get_typeinfo_from_locator(*locator_ptr);
}

HANDLE WindowsProcess::create_remote_thread(uintptr_t address, uintptr_t param) {
    return CreateRemoteThread(m_process, nullptr, 0, (LPTHREAD_START_ROUTINE)address, (LPVOID)param, 0, nullptr);
}

std::optional<std::string> WindowsProcess::get_typename(uintptr_t ptr) {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto vtable = Process::read<uintptr_t>(ptr);

    if (!vtable || *vtable == 0) {
        return std::nullopt;
    }

    return get_typename_from_vtable(*vtable);
}

std::optional<std::string> WindowsProcess::get_typename_from_vtable(uintptr_t ptr) try {
    if (ptr == 0) {
        return std::nullopt;
    }

    auto typeinfo = try_get_typeinfo_from_vtable(ptr);

    if (!typeinfo) {
        return std::nullopt;
    }

    auto ti = (std::type_info*)&*typeinfo;
    if (ti->raw_name()[0] != '.' || ti->raw_name()[1] != '?') {
        return std::nullopt;
    }

    if (std::string_view{ti->raw_name()}.find("@") == std::string_view::npos) {
        return std::nullopt;
    }

    auto raw_data = (__std_type_info_data*)((uintptr_t)ti + sizeof(void*));
    raw_data->_UndecoratedName = nullptr; // fixes a crash if memory already allocated because it's not allocated by us

    const auto result = std::string_view{ti->name()};

    if (result.empty() || result == " ") {
        return std::nullopt;
    }

    return std::string{result};
} catch(...) {
    return std::nullopt;
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

bool WindowsProcess::derives_from(uintptr_t obj_ptr, const std::string_view& type_name) {
    if (obj_ptr == 0) {
        return false;
    }

    // Read vtable pointer from object
    auto vtable_opt = Process::read<uintptr_t>(obj_ptr);
    if (!vtable_opt) {
        return false;
    }
    uintptr_t vtable = *vtable_opt;

    // Read complete object locator pointer from before vtable
    auto locator_ptr_opt = Process::read<uintptr_t>(vtable - sizeof(void*));
    if (!locator_ptr_opt || *locator_ptr_opt == 0) {
        return false;
    }
    uintptr_t locator_ptr = *locator_ptr_opt;

    // Read the complete object locator
    auto locator_opt = Process::read<_s_RTTICompleteObjectLocator>(locator_ptr);
    if (!locator_opt) {
        return false;
    }
    const auto& locator = *locator_opt;

    // Get module base address
    auto module_within = get_module_within(locator_ptr);
    if (!module_within) {
        return false;
    }
    uintptr_t module_base = module_within->start;

    // Read class hierarchy descriptor
#if _RTTI_RELATIVE_TYPEINFO
    uintptr_t class_hierarchy_ptr = module_base + locator.pClassDescriptor;
#else
    uintptr_t class_hierarchy_ptr = (uintptr_t)locator.pClassDescriptor;
#endif

    auto class_hierarchy_opt = Process::read<_s_RTTIClassHierarchyDescriptor>(class_hierarchy_ptr);
    if (!class_hierarchy_opt) {
        return false;
    }
    const auto& class_hierarchy = *class_hierarchy_opt;

    // Read base class array
#if _RTTI_RELATIVE_TYPEINFO
    uintptr_t base_classes_ptr = module_base + class_hierarchy.pBaseClassArray;
#else
    uintptr_t base_classes_ptr = (uintptr_t)class_hierarchy.pBaseClassArray;
#endif

    // Iterate through base classes
    for (auto i = 0; i < class_hierarchy.numBaseClasses; ++i) {
        // Read base class descriptor pointer
        auto desc_offset_ptr = base_classes_ptr + i * sizeof(uintptr_t);
#if _RTTI_RELATIVE_TYPEINFO
        auto desc_offset_opt = Process::read<int>(desc_offset_ptr);
        if (!desc_offset_opt || *desc_offset_opt == 0) {
            continue;
        }
        uintptr_t desc_ptr = module_base + *desc_offset_opt;
#else
        auto desc_ptr_opt = Process::read<uintptr_t>(desc_offset_ptr);
        if (!desc_ptr_opt || *desc_ptr_opt == 0) {
            continue;
        }
        uintptr_t desc_ptr = *desc_ptr_opt;
#endif

        // Read base class descriptor
        auto desc_opt = Process::read<_s_RTTIBaseClassDescriptor>(desc_ptr);
        if (!desc_opt) {
            continue;
        }
        const auto& desc = *desc_opt;

        // Read type descriptor
#if _RTTI_RELATIVE_TYPEINFO
        uintptr_t ti_ptr = module_base + desc.pTypeDescriptor;
#else
        uintptr_t ti_ptr = (uintptr_t)desc.pTypeDescriptor;
#endif

        // Read typeinfo
        auto typeinfo_buf = Process::read<std::array<uint8_t, sizeof(std::type_info) + 256>>(ti_ptr);
        if (!typeinfo_buf) {
            continue;
        }

        // Access the typeinfo
        auto ti = reinterpret_cast<std::type_info*>(&(*typeinfo_buf)[0]);
        
        // Validate the raw name format
        const char* raw_name = ti->raw_name();
        if (raw_name[0] != '.' || raw_name[1] != '?') {
            return false; // Invalid format
        }

        if (std::string_view{raw_name}.find("@") == std::string_view::npos) {
            return false; // Invalid format
        }

        // Clear the undecorated name pointer to avoid crashes
        auto raw_data = reinterpret_cast<__std_type_info_data*>((uintptr_t)ti + sizeof(void*));
        raw_data->_UndecoratedName = nullptr;

        // Check if the type name matches
        const auto result = std::string_view{ti->name()};
        if (result == type_name) {
            return true;
        }
    }

    return false;
}

bool WindowsProcess::derives_from(uintptr_t obj_ptr, const std::array<uint8_t, sizeof(std::type_info) + 256>& ti_compare) {
    if (obj_ptr == 0) {
        return false;
    }

    // Read vtable pointer from object
    auto vtable_opt = Process::read<uintptr_t>(obj_ptr);
    if (!vtable_opt) {
        return false;
    }
    uintptr_t vtable = *vtable_opt;

    // Read complete object locator pointer from before vtable
    auto locator_ptr_opt = Process::read<uintptr_t>(vtable - sizeof(void*));
    if (!locator_ptr_opt || *locator_ptr_opt == 0) {
        return false;
    }
    uintptr_t locator_ptr = *locator_ptr_opt;

    // Read the complete object locator
    auto locator_opt = Process::read<_s_RTTICompleteObjectLocator>(locator_ptr);
    if (!locator_opt) {
        return false;
    }
    const auto& locator = *locator_opt;

    // Get module base address
    auto module_within = get_module_within(locator_ptr);
    if (!module_within) {
        return false;
    }
    uintptr_t module_base = module_within->start;

    // Read class hierarchy descriptor
#if _RTTI_RELATIVE_TYPEINFO
    uintptr_t class_hierarchy_ptr = module_base + locator.pClassDescriptor;
#else
    uintptr_t class_hierarchy_ptr = (uintptr_t)locator.pClassDescriptor;
#endif

    auto class_hierarchy_opt = Process::read<_s_RTTIClassHierarchyDescriptor>(class_hierarchy_ptr);
    if (!class_hierarchy_opt) {
        return false;
    }
    const auto& class_hierarchy = *class_hierarchy_opt;

    // Read base class array
#if _RTTI_RELATIVE_TYPEINFO
    uintptr_t base_classes_ptr = module_base + class_hierarchy.pBaseClassArray;
#else
    uintptr_t base_classes_ptr = (uintptr_t)class_hierarchy.pBaseClassArray;
#endif

    // Extract the type_info from the comparison buffer for comparison
    auto ti_compare_ptr = reinterpret_cast<const std::type_info*>(&ti_compare[0]);

    // Iterate through base classes
    for (auto i = 0; i < class_hierarchy.numBaseClasses; ++i) {
        // Read base class descriptor pointer
        auto desc_offset_ptr = base_classes_ptr + i * sizeof(uintptr_t);
#if _RTTI_RELATIVE_TYPEINFO
        auto desc_offset_opt = Process::read<int>(desc_offset_ptr);
        if (!desc_offset_opt || *desc_offset_opt == 0) {
            continue;
        }
        uintptr_t desc_ptr = module_base + *desc_offset_opt;
#else
        auto desc_ptr_opt = Process::read<uintptr_t>(desc_offset_ptr);
        if (!desc_ptr_opt || *desc_ptr_opt == 0) {
            continue;
        }
        uintptr_t desc_ptr = *desc_ptr_opt;
#endif

        // Read base class descriptor
        auto desc_opt = Process::read<_s_RTTIBaseClassDescriptor>(desc_ptr);
        if (!desc_opt) {
            continue;
        }
        const auto& desc = *desc_opt;

        // Read type descriptor
#if _RTTI_RELATIVE_TYPEINFO
        uintptr_t ti_ptr = module_base + desc.pTypeDescriptor;
#else
        uintptr_t ti_ptr = (uintptr_t)desc.pTypeDescriptor;
#endif

        // Read typeinfo
        auto typeinfo_buf = Process::read<std::array<uint8_t, sizeof(std::type_info) + 256>>(ti_ptr);
        if (!typeinfo_buf) {
            continue;
        }        // Compare type_info raw buffer
        if (std::memcmp(&(*typeinfo_buf)[0], &ti_compare[0], sizeof(std::type_info)) == 0) {
            // Verify the raw name matches as well
            auto ti = reinterpret_cast<std::type_info*>(&(*typeinfo_buf)[0]);
            auto ti_compare_ptr = reinterpret_cast<const std::type_info*>(&ti_compare[0]);
            
            if (std::string_view{ti->raw_name()} == std::string_view{ti_compare_ptr->raw_name()}) {
                return true;
            }
        }
    }

    return false;
}

std::vector<uintptr_t> WindowsProcess::get_objects_of_type(uintptr_t start, size_t size, const std::string_view& type_name) {
    std::vector<uintptr_t> results;
    
    // Validate parameters
    if (start == 0 || size == 0) {
        return results;
    }
    
    // Clone the memory region
    std::vector<uint8_t> memory_data(size);
    
    if (!read(start, memory_data.data(), memory_data.size())) {
        return results;
    }
    
    // Scan memory for RTTI objects
    size_t ptr_size = sizeof(void*);
    
    // Start scanning at aligned addresses
    for (size_t i = 0; i <= memory_data.size() - ptr_size; i += ptr_size) {
        // Get pointer value from memory
        uintptr_t ptr_value = 0;
        std::memcpy(&ptr_value, memory_data.data() + i, ptr_size);
        
        // Skip null or obviously invalid pointers
        if (ptr_value == 0 || ptr_value < 0x10000) {
            continue;
        }
        
        // Check both the pointer itself and the value it points to
        for (size_t j = 0; j < 2; ++j) {
            const auto tname = get_typename(j == 0 ? start + i : ptr_value);
            
            if (!tname || tname->empty()) {
                continue;
            }
            
            // Filter based on type_name
            if (tname->find(type_name) != std::string::npos) {
                results.push_back(start + i);
                break; // Found a match, no need to check the other case
            }
        }
    }
    
    return results;
}
} // namespace arch
