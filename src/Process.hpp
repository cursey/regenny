#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Module.hpp"

class Process {
public:
    virtual bool read(uintptr_t address, void* buffer, size_t size) = 0;
    virtual bool write(uintptr_t address, const void* buffer, size_t size) = 0;
    virtual uint32_t process_id() = 0;
    virtual std::vector<std::unique_ptr<Module>> modules() = 0;
    virtual bool ok() = 0;
};