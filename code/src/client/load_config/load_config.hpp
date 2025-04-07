#ifndef LOAD_CONFIG_HPP
#define LOAD_CONFIG_HPP

#include <string>
#include <nlohmann/json.hpp>
#include "../logger/Mylogger.hpp"

using json = nlohmann::json;

namespace ConfigReader
{
    json load(const std::string &filepath);
    bool save(const std::string &filepath, const json &j);
    int get_config_value(const std::string &key, const json &j);
    std::string get_config_string(const std::string &key, const json &j);
    unsigned short get_config_short(const std::string &key, const json &j);
};

#endif // LOAD_CONFIG_HPP