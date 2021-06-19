#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace node {
struct Property {
    std::variant<std::monostate, bool, int> value{};
    std::unordered_map<std::string, Property> props{};

    Property* find(const std::string& s) {
        if (auto search = props.find(s); search != props.end()) {
            return &search->second;
        }

        return nullptr;
    }

    Property& operator[](const std::string& s) { return props[s]; }
};
} // namespace node