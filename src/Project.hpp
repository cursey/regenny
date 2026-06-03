#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "node/Property.hpp"

struct TypeTab {
    std::string name{};
    std::string type_name{};
    std::string address{};
    
    bool operator==(const TypeTab& other) const {
        return name == other.name && type_name == other.type_name && address == other.address;
    }
};

struct Project {
    std::string extension_header{".hpp"};
    std::string extension_source{".cpp"};
    std::string process_filter{};
    uint32_t process_id{};
    std::string process_name{};
    std::map<std::string, node::Property> props{};
    std::map<std::string, std::string> type_addresses{};
    std::string type_chosen{};
    
    // Tab system
    std::vector<TypeTab> tabs{};
    int active_tab_index{-1};
};

void to_json(nlohmann::json& j, const Project& p);
void from_json(const nlohmann::json& j, Project& p);
