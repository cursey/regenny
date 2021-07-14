#include "Project.hpp"

void to_json(nlohmann::json& j, const Project& p) {
    std::function<void(nlohmann::json&, const std::string&, const node::Property&)> visit =
        [&visit](nlohmann::json& j, const std::string& name, const node::Property& prop) {
            if (prop.value.index() == 0 && prop.props.empty()) {
                return;
            }

            if (prop.value != prop.default_value) {
                std::visit(
                    [&](auto&& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, int>) {
                            j[name] = value;
                        } else if constexpr (std::is_same_v<T, bool>) {
                            j[name] = value;
                        }
                    },
                    prop.value);
            }

            for (auto&& [child_name, child_prop] : prop.props) {
                visit(j[name], child_name, child_prop);
            }
        };

    std::function<void(nlohmann::json&)> erase_null = [&erase_null](nlohmann::json& j) {
        if (!j.is_object()) {
            return;
        }

        for (auto it = j.begin(); it != j.end();) {
            if (it->is_null()) {
                it = j.erase(it);
            } else {
                erase_null(it.value());
                ++it;
            }
        }
    };

    j["props"].clear();

    for (auto&& [type_name, props] : p.props) {
        visit(j["props"], type_name, props);
    }

    for (auto it = j["props"].begin(); it != j["props"].end(); ++it) {
        erase_null(it.value());
    }

    j["chosen_type"] = p.chosen_type;
    j["process_filter"] = p.process_filter;
    j["process_id"] = p.process_id;
    j["process_name"] = p.process_name;
    j["extension_header"] = p.extension_header;
    j["extension_source"] = p.extension_source;
    j["type_addresses"] = p.type_addresses;
}

void from_json(const nlohmann::json& j, Project& p) {
    p.chosen_type = j.value("chosen_type", "");
    p.process_filter = j.value("process_filter", "");
    p.process_id = j.value<uint32_t>("process_id", 0);
    p.process_name = j.value("process_name", "");
    p.extension_header = j.value("extension_header", ".hpp");
    p.extension_source = j.value("extension_source", ".cpp");
    p.type_addresses = j.value<decltype(p.type_addresses)>("type_addresses", {});
    p.props.clear();

    std::function<void(const nlohmann::json&, node::Property&)> visit = [&visit](const nlohmann::json& j,
                                                                            node::Property& prop) {
        if (j.empty()) {
            return;
        }

        for (auto it = j.begin(); it != j.end(); ++it) {
            auto& val = it.value();

            if (val.is_boolean()) {
                prop[it.key()].value = (bool)val;
            } else if (val.is_number()) {
                prop[it.key()].value = (int)val;
            } else if (val.is_object()) {
                visit(val, prop.props[it.key()]);
            }
        }
    };

    for (auto it = j["props"].begin(); it != j["props"].end(); ++it) {
        visit(it.value(), p.props[it.key()]);
    }
}
