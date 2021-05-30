#pragma once

#include <cstdint>
#include <map>
#include <string>

class Helpers {
public:
    virtual std::map<std::string, uint32_t> processes() = 0;
};