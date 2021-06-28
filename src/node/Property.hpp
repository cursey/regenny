#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace node {
struct Property {
    std::variant<std::monostate, bool, int> value{};
    std::variant<std::monostate, bool, int> default_value{};
    std::unordered_map<std::string, Property> props{};

    Property* find(const std::string& s) {
        if (auto search = props.find(s); search != props.end()) {
            return &search->second;
        }

        return nullptr;
    }

    Property& operator[](const std::string& s) { return props[s]; }

    template <typename T> void set(T val) { value = val; }

    template <typename T> void set_default(T val) {
        if (value.index() == 0) {
            value = val;
        }

        default_value = val;
    }

    bool& as_bool() { return std::get<bool>(value); }
    int& as_int() { return std::get<int>(value); }
};
} // namespace node