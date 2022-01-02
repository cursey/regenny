#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct ParsedAddress {
    std::string name{};
    std::vector<uintptr_t> offsets{};
};

std::optional<ParsedAddress> parse_address(const std::string& str);