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

    bool write(uintptr_t address, const void* buffer, size_t size) override;
    uint32_t process_id() override;
    bool ok() override { return m_process != nullptr; }

    std::optional<std::string> get_typename(uintptr_t ptr) override;

    // RTTI
    std::optional<uintptr_t> get_complete_object_locator_ptr(uintptr_t ptr);
    std::optional<_s_RTTICompleteObjectLocator> get_complete_object_locator(uintptr_t ptr);
    std::optional<std::array<uint8_t, sizeof(std::type_info) + 256>> get_typeinfo(uintptr_t ptr); // __RTtypeid

protected:
    bool handle_read(uintptr_t address, void* buffer, size_t size) override;

private:
    HANDLE m_process{};
};

class WindowsHelpers : public Helpers {
public:
    std::map<uint32_t, std::string> processes() override;
};
} // namespace arch
