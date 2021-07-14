#include "Config.hpp"

void to_json(nlohmann::json& j, const Config& c) {
    j["font"]["file"] = c.font_file;
    j["font"]["size"] = c.font_size;
    j["file"]["history"] = c.file_history;
}

void from_json(const nlohmann::json& j, Config& c) {
    c.font_file = j.at("font").value("file", "");
    c.font_size = j.at("font").value("size", 16.0f);
    c.file_history = j.at("file").value<decltype(c.file_history)>("history", {});
}
