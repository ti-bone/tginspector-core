/*
 * config/config.cpp
 * Core of TgInspector, namespace for loading configuration, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#include <boost/property_tree/ptree.hpp>
#include <string>
#include "config.hpp"

boost::property_tree::ptree Config::read_cfg(const std::string &path) {
    boost::property_tree::ptree cfg;
    boost::property_tree::read_json(path, cfg, std::locale());
    return cfg;
}
