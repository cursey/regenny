#pragma once

#include <array>

#include <Windows.h>
#include <rttidata.h>
#include <vcruntime_typeinfo.h>

#include "Helpers.hpp"
#include "Process.hpp"

namespace arch {
class WindowsProcess : public Process {
public:
    WindowsProcess(DWORD process_id);

    uint32_t process_id() override;
    bool ok() override;

    std::optional<std::string> get_typename(uintptr_t ptr) override;
    std::optional<std::string> get_typename_from_vtable(uintptr_t ptr) override;

    // RTTI
    std::optional<uintptr_t> get_complete_object_locator_ptr_from_vtable(uintptr_t vtable);
    std::optional<uintptr_t> get_complete_object_locator_ptr(uintptr_t ptr);
    std::optional<_s_RTTICompleteObjectLocator> get_complete_object_locator(uintptr_t ptr);
    std::optional<uintptr_t> resolve_object_base_address(uintptr_t ptr); // subtracts value of RTTICompleteObjectLocator.offset
    
    std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> try_get_typeinfo_from_locator(uintptr_t locator_ptr);
    std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> try_get_typeinfo_from_ptr(uintptr_t ptr);
    std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> try_get_typeinfo_from_vtable(uintptr_t vtable);

    // Inheritance checking
    bool derives_from(uintptr_t obj_ptr, const std::string_view& type_name);
    bool derives_from(uintptr_t obj_ptr, const std::array<uint8_t, sizeof(std::type_info) + 256>& ti_compare);

    HANDLE create_remote_thread(uintptr_t address, uintptr_t param);

protected:
    bool handle_write(uintptr_t address, const void* buffer, size_t size) override;
    bool handle_read(uintptr_t address, void* buffer, size_t size) override;
    std::optional<uint64_t> handle_protect(uintptr_t address, size_t size, uint64_t flags) override;
    std::optional<uintptr_t> handle_allocate(uintptr_t address, size_t size, uint64_t flags) override;

private:
    HANDLE m_process{};
};

class WindowsHelpers : public Helpers {
public:
    std::map<uint32_t, std::string> processes() override;
};
} // namespace arch
