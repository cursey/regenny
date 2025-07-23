#pragma once

#include <array>
#include <sys/types.h>

#include "Helpers.hpp"
#include "Process.hpp"

namespace arch {
class LinuxProcess : public Process {
public:
    LinuxProcess(pid_t process_id);
    ~LinuxProcess() override = default;

    uint32_t process_id() override;
    bool ok() override;

    std::optional<std::string> get_typename(uintptr_t ptr) override;
    std::optional<std::string> get_typename_from_vtable(uintptr_t ptr) override;

protected:
    bool handle_write(uintptr_t address, const void* buffer, size_t size) override;
    bool handle_read(uintptr_t address, void* buffer, size_t size) override;
    std::optional<uint64_t> handle_protect(uintptr_t address, size_t size, uint64_t flags) override;
    std::optional<uintptr_t> handle_allocate(uintptr_t address, size_t size, uint64_t flags) override;

private:
    pid_t m_process{};
    int rw_handle;
};

class LinuxHelpers : public Helpers {
public:
    ~LinuxHelpers() override = default;

    std::map<uint32_t, std::string> processes() override;
};
} // namespace arch
