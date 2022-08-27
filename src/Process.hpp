#pragma once

#include <cstdint>
#include <optional>
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

    struct ReadOnlyAllocation : public Allocation {
        // Read only allocations get cached.
        std::vector<std::byte> mem{};
    };

    bool read(uintptr_t address, void* buffer, size_t size);
    bool write(uintptr_t address, const void* buffer, size_t size);
    std::optional<uint64_t> protect(uintptr_t address, size_t size, uint64_t flags);
    std::optional<uintptr_t> allocate(uintptr_t address, size_t size, uint64_t flags);
    virtual uint32_t process_id() { return 0; }
    virtual bool ok() { return true; }

    // RTTI
    virtual std::optional<std::string> get_typename(uintptr_t ptr) { return std::nullopt; }

    auto&& modules() const { return m_modules; }
    auto&& allocations() const { return m_allocations; }

    const Process::Module* get_module_within(uintptr_t addr) const;
    const Process::Module* get_module(std::string_view name) const;

    template <typename T> std::optional<T> read(uintptr_t address) {
        T out{};

        if (!read(address, &out, sizeof(T))) {
            return std::nullopt;
        }

        return out;
    }

    template <typename T> bool write(uintptr_t address, const T& value) {
        return write(address, &value, sizeof(T));
    }

protected:
    std::vector<Module> m_modules{};
    std::vector<Allocation> m_allocations{};
    std::vector<ReadOnlyAllocation> m_read_only_allocations{};

    virtual bool handle_write(uintptr_t address, const void* buffer, size_t size) { return true; }
    virtual bool handle_read(uintptr_t address, void* buffer, size_t size) { return true; }
    virtual std::optional<uint64_t> handle_protect(uintptr_t address, size_t size, uint64_t flags) { return std::nullopt; }
    virtual std::optional<uintptr_t> handle_allocate(uintptr_t address, size_t size, uint64_t flags) { return std::nullopt; }
};