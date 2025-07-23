#pragma once

#include <cstdint>
#include <map>
#include <string>

class Helpers {
public:
    virtual ~Helpers() = default;
    virtual std::map<uint32_t, std::string> processes() = 0;
};
