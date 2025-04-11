#include "load_config.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace ConfigReader
{
    json load(const std::string &filepath)
    {
        std::ifstream config_file(filepath);
        if (!config_file.is_open())
        {
            MyLogger::error("Unable to open configuration file: " + filepath);
            throw std::runtime_error("Could not open config file: " + filepath);
        }

        try
        {
            json j;
            config_file >> j;
            MyLogger::info("Configuration file loaded successfully: " + filepath);
            MyLogger::debug("Loaded JSON: " + j.dump(4)); // Pretty print the JSON
            return j;
        }
        catch (const json::parse_error &e)
        {
            MyLogger::error("JSON parse error in file " + filepath + ": " + e.what());
        }
        return json();
    }

    bool save(const std::string &filepath, const json &j)
    {
        std::ofstream config_file(filepath);
        if (!config_file.is_open())
        {
            MyLogger::error("Unable to open configuration file for writing: " + filepath);
            throw std::runtime_error("Could not open config file for writing: " + filepath);
        }

        try
        {
            config_file << j.dump(4); // Indent with 4 spaces for readability
            MyLogger::info("Configuration file saved successfully: " + filepath);
            return true;
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Error saving JSON to file " + filepath + ": " + e.what());
            throw;
        }
        return false;
    }

    int get_config_value(const std::string &key, const json &j)
    {
        try
        {
            if (!j.contains(key))
            {
                MyLogger::error("Key not found in JSON: " + key);
                return 0;
            }
            if (!j[key].is_number_integer())
            {
                MyLogger::error("Key is not an integer: " + key);
                return 0;
            }
            return j[key].get<int>();
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Error retrieving int value for key '" + key + "': " + e.what());
            return 0;
        }
    }

    std::string get_config_string(const std::string &key, const json &j)
    {
        try
        {
            if (!j.contains(key))
            {
                MyLogger::error("Key not found in JSON: " + key);
                return "";
            }
            if (!j[key].is_string())
            {
                MyLogger::error("Key is not a string: " + key);
                return "";
            }
            return j[key].get<std::string>();
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Error retrieving string value for key '" + key + "': " + e.what());
            return "";
        }
    }

    unsigned short get_config_short(const std::string &key, const json &j)
    {
        try
        {
            if (!j.contains(key))
            {
                MyLogger::error("Key not found in JSON: " + key);
                return 0;
            }
            if (!j[key].is_number_unsigned())
            {
                MyLogger::error("Key is not an unsigned integer: " + key);
                return 0;
            }
            auto val = j[key].get<unsigned int>();
            if (val > std::numeric_limits<unsigned short>::max())
            {
                MyLogger::error("Value for key '" + key + "' exceeds unsigned short limit");
                return 0;
            }
            return static_cast<unsigned short>(val);
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Error retrieving unsigned short value for key '" + key + "': " + e.what());
            return 0;
        }
    }
}
