#pragma once

#include <cstdint>
#include <string>

class Module {
public:
    virtual const char* name() = 0;
    virtual uintptr_t address() = 0;
    virtual size_t size() = 0;
};
