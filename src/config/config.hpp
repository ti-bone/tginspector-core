#pragma once

#include <iostream>
#include <boost/property_tree/json_parser.hpp>

class Config {
public:
    inline static std::string default_cfg_path = "config.json";
    static boost::property_tree::ptree read_cfg(const std::string& path);
};