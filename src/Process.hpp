#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Process {
public:
    class Module {
    public:
        std::string name{};
        uintptr_t start{};
        uintptr_t end{};
        size_t size{};
    };

    struct Allocation {
        uintptr_t start{};
        uintptr_t end{};
        size_t size{};

        bool read{};
        bool write{};
        bool execute{};
    };

    virtual bool read(uintptr_t address, void* buffer, size_t size) = 0;
    virtual bool write(uintptr_t address, const void* buffer, size_t size) = 0;
    virtual uint32_t process_id() = 0;
    virtual bool ok() = 0;

    auto&& modules() const { return m_modules; }
    auto&& allocations() const { return m_allocations; }

protected:
    std::vector<Module> m_modules{};
    std::vector<Allocation> m_allocations{};
};