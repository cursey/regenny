#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

struct ParsedAddress {
    std::string name{};
    std::vector<uintptr_t> offsets{};
};

std::optional<ParsedAddress> parse_address(const std::string& str);