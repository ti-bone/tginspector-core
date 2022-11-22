/*
 * sql/sql.hpp
 * Core of TgInspector, namespace for working with DB, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#pragma once

#include <iostream>
#include "../config/config.hpp"

// postgres
#include <pqxx/pqxx>

// mysql
#include <jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>
#include <jdbc/cppconn/prepared_statement.h>

// Logging
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/stopwatch.h"

// FS
#include <filesystem>

namespace fs = std::filesystem;

class Sql {
private:
	std::string host = "tcp://192.168.1.109:3306";
	std::string user = "tgdb";
	std::string pass = ";hFU;3nf35";
	std::string db   = "tgscrapper";
public:
	static void writeMessage(
		const std::string& chat_id,
		const std::string& chat_name,
		const std::string& message_id,
		const std::string& message_timestamp,
		const std::string& sender_id,
        const std::string& first_name,
		const std::string& last_name,
		const std::string& text,
		const std::string& file_id,
        const std::string& file_type
	);

	static void writeFileInfo(
        const std::string& file_id,
        const std::string& file_name
	);
	
	static void writeUserName(
        const std::string& sender_id,
        const std::string& username
	);

    static void writeLocation(
        const std::string& chat_id,
        const std::string& chat_name,
        const std::string& message_id,
        const std::string& message_timestamp,
        const std::string& sender_id,
        const std::string& first_name,
        const std::string& last_name,
        const std::string& latitude,
        const std::string& longitude,
        const std::string& accuracy,
        const std::string& heading
    );

    static bool checkFile(
            const std::string& file_id
    );

    static void writeAvatar(
            const std::string& file_id,
            const std::string& user_id,
            const std::string& timestamp
    );
};