#include "Config.hpp"

void to_json(nlohmann::json& j, const Config& c) {
    j["font"]["file"] = c.font_file;
    j["font"]["size"] = c.font_size;
    j["file"]["history"] = c.file_history;
    j["display"]["address"] = c.display_address;
    j["display"]["offset"] = c.display_offset;
    j["display"]["bytes"] = c.display_bytes;
    j["display"]["print"] = c.display_print;
}

void from_json(const nlohmann::json& j, Config& c) {
    if (j.find("font") != j.end()) {
        c.font_file = j.at("font").value("file", "");
        c.font_size = j.at("font").value("size", 16.0f);
    }

    if (j.find("file") != j.end()) {
        c.file_history = j.at("file").value<decltype(c.file_history)>("history", {});
    }

    if (j.find("display") != j.end()) {
        c.display_address = j.at("display").value("address", true);
        c.display_offset = j.at("display").value("offset", true);
        c.display_bytes = j.at("display").value("bytes", true);
        c.display_print = j.at("display").value("print", true);
    }
}
