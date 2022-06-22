#pragma once

#include <deque>
#include <string>

#include <nlohmann/json.hpp>

struct Config {
    std::string font_file{};
    float font_size{16.0f};
    std::deque<std::string> file_history{};
    bool display_address{true};
    bool display_offset{true};
    bool display_bytes{true};
    bool display_print{true};
};

void to_json(nlohmann::json& j, const Config& c);
void from_json(const nlohmann::json& j, Config& c);
