/*
 * sql/writeLocation.cpp
 * Core of TgInspector, namespace for working with DB, function for writing location in DB, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#include "sql.hpp"

void Sql::writeLocation(const std::string& chat_id, const std::string& chat_name, const std::string& message_id,
                       const std::string& message_timestamp, const std::string& sender_id, const std::string& first_name,
                       const std::string& last_name, const std::string& latitude, const std::string& longitude,
                       const std::string& accuracy, const std::string& heading)
{
    int dbs = 0;
    spdlog::stopwatch total_sw;

    // Read cfg
    boost::property_tree::ptree cfg = Config::read_cfg(Config::default_cfg_path);

    // make "fingerprint" for anti-duplication messages in the db
    std::string UFP = chat_id + message_id;

    if (cfg.get<bool>("tdlib.test_mode")) spdlog::set_level(spdlog::level::debug);

    // postgresql
    if (cfg.get<bool>("db.postgresql.enabled")) {
        try {
            spdlog::stopwatch global_sw;
            spdlog::stopwatch pqconn_sw;
            // Connect to a DB
            pqxx::connection pqconn(cfg.get<std::string>("db.postgresql.host"));

            spdlog::debug("<PostgreSQL> Executing write location...");
            spdlog::debug("<PostgreSQL> Connected to a DB: {} [Server: {}, Port: {}].", pqconn.dbname(), pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Connection took {} seconds.", pqconn_sw);

            spdlog::stopwatch query_sw;

            // Init transaction
            pqxx::work transaction{pqconn};

            // Execute SQL query
            transaction.exec("insert into " + cfg.get<std::string>("db.postgresql.locations_table") +
                             " values ('" +
                             transaction.esc(UFP) +
                             "', " + transaction.esc(chat_id) +
                             ", '" + transaction.esc(chat_name) +
                             "', " + transaction.esc(message_id) +
                             ", " + transaction.esc(message_timestamp) +
                             ", " + transaction.esc(sender_id) +
                             ", '" + transaction.esc(first_name) +
                             "', '" + transaction.esc(last_name) +
                             "', " + transaction.esc(latitude) +
                             ", " + transaction.esc(longitude) +
                             ", " + transaction.esc(accuracy) +
                             ", " + transaction.esc(heading) + ")");

            spdlog::debug("<PostgreSQL> INSERT Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.locations_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", query_sw);
            spdlog::debug("<PostgreSQL> Committing transaction...");

            // Commit transaction
            transaction.commit();

            spdlog::debug("<PostgreSQL> Transaction committed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.locations_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL> Message ID of location, that was written: {}.", message_id);
            spdlog::debug("<PostgreSQL, Perf> Writing location in DB took {} seconds.", global_sw);
            dbs++;
        } catch (std::exception const &e) {
            std::cerr << e.what() << std::endl;
        }
    }

    if (cfg.get<bool>("db.mysql.enabled")) {
        // mysql
        try {
            spdlog::stopwatch global_sw;
            spdlog::stopwatch conn_sw;
            sql::mysql::MySQL_Driver *driver;
            sql::Connection *con;
            sql::PreparedStatement *pstmt;

            // init mysql driver
            driver = sql::mysql::get_mysql_driver_instance();

            // connect to mysql instance
            con = driver->connect(cfg.get<std::string>("db.mysql.host"),
                                  cfg.get<std::string>("db.mysql.user"),
                                  cfg.get<std::string>("db.mysql.pass"));

            // set db
            con->setSchema(cfg.get<std::string>("db.mysql.database"));

            spdlog::debug("<MySQL> Executing write location...");
            spdlog::debug("<MySQL> Connected to a DB: {} [Server: {}].", con->getSchema().asStdString(), cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Connecting to DB took {} seconds.", conn_sw);

            spdlog::stopwatch query_sw;

            // write data in locations table
            // prepare query
            pstmt = con->prepareStatement("INSERT INTO " + cfg.get<std::string>("db.mysql.locations_table") +
                                          "(`fp`, `chat_id`, `chat_name`, `message_id`, `message_timestamp`, `sender_id`, `first_name`,"
                                          " `last_name`, `latitude`, `longitude`, `accuracy`, `heading`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ");

            // set values
            pstmt->setString(1, UFP);
            pstmt->setString(2, chat_id);
            pstmt->setString(3, chat_name);
            pstmt->setString(4, message_id);
            pstmt->setString(5, message_timestamp);
            pstmt->setString(6, sender_id);
            pstmt->setString(7, first_name);
            pstmt->setString(8, last_name);
            pstmt->setString(9, latitude);
            pstmt->setString(10, longitude);
            pstmt->setString(11, accuracy);
            pstmt->setString(12, heading);

            // execute query
            pstmt->execute();

            spdlog::debug("<MySQL> INSERT Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                          cfg.get<std::string>("db.mysql.locations_table"),
                          cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL> Message ID of location, that was written: {}.", message_id);
            spdlog::debug("<MySQL, Perf> Executing INSERT Query took {} seconds.", query_sw);
            spdlog::debug("<MySQL, Perf> Writing location in DB took {} seconds.", global_sw);
            delete pstmt;
            delete con;
            dbs++;
        } catch (sql::SQLException &e) {
            std::cout << "# ERR: SQLException in " << __FILE__;
            std::cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << std::endl;
            std::cout << "# ERR: " << e.what();
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }
    }

    spdlog::debug("<Perf> Writing location in {} DB(s) took {} seconds.", dbs, total_sw);
}