#pragma once

#include <cstdint>
#include <map>
#include <string>

class Helpers {
public:
    virtual std::map<uint32_t, std::string> processes() = 0;
};