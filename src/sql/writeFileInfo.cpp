/*
 * sql/writeFileInfo.cpp
 * Core of TgInspector, namespace for working with DB, function for writing info about file(it's location and etc) in DB.
 * Do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#include "sql.hpp"

void Sql::writeFileInfo(const std::string& file_id, const std::string& file_name)
{
    int dbs = 0;
    spdlog::stopwatch total_sw;

    // Read cfg
    boost::property_tree::ptree cfg = Config::read_cfg(Config::default_cfg_path);

    if (cfg.get<bool>("tdlib.test_mode")) spdlog::set_level(spdlog::level::debug);

    // postgresql
    if (cfg.get<bool>("db.postgresql.enabled")) {
        try {
            spdlog::stopwatch global_sw;
            spdlog::stopwatch pqconn_sw;

            // Connect to a DB
            pqxx::connection pqconn(cfg.get<std::string>("db.postgresql.host"));

            spdlog::debug("<PostgreSQL> Executing write file info...");
            spdlog::debug("<PostgreSQL> Connected to a DB: {} [Server: {}, Port: {}].", pqconn.dbname(), pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Connection took {} seconds.", pqconn_sw);

            spdlog::stopwatch query_sw;

            // Init transaction
            pqxx::work transaction{pqconn};

            // Execute SQL query
            transaction.exec("insert into " + cfg.get<std::string>("db.postgresql.files_table") +
                             " values ('" + transaction.esc(file_id) + "', '" + transaction.esc(file_name) + "')");

            spdlog::debug("<PostgreSQL> INSERT Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.files_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", query_sw);
            spdlog::debug("<PostgreSQL> Committing transaction...");

            // Commit transaction
            transaction.commit();

            spdlog::debug("<PostgreSQL> Transaction committed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.files_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL> ID of file, that was written: {}.", file_id);
            spdlog::debug("<PostgreSQL, Perf> Writing file info in DB took {} seconds.", global_sw);
            dbs++;
        } catch (std::exception const &e) {
            std::cerr << e.what() << std::endl;
        }
    }

    // mysql
    if (cfg.get<bool>("db.mysql.enabled")) {
        try {
            spdlog::stopwatch global_sw;
            spdlog::stopwatch conn_sw;
            sql::mysql::MySQL_Driver* driver;
            sql::Connection* con;
            sql::PreparedStatement* pstmt;

            // init mysql driver
            driver = sql::mysql::get_mysql_driver_instance();

            // connect to mysql instance
            con = driver->connect(cfg.get<std::string>("db.mysql.host"), cfg.get<std::string>("db.mysql.user"), cfg.get<std::string>("db.mysql.pass"));

            // set db
            con->setSchema(cfg.get<std::string>("db.mysql.database"));

            spdlog::debug("<MySQL> Executing write file info...");
            spdlog::debug("<MySQL> Connected to a DB: {} [Server: {}].", con->getSchema().asStdString(), cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Connecting to DB took {} seconds.", conn_sw);

            spdlog::stopwatch query_sw;

            // write data in files table
            // prepare query
            pstmt = con->prepareStatement("INSERT INTO " + cfg.get<std::string>("db.mysql.files_table") + " (`file_id`, `file_name`) VALUES (?, ?) ");

            // set values
            pstmt->setString(1, file_id);
            pstmt->setString(2, file_name);

            // execute query
            pstmt->execute();

            spdlog::debug("<MySQL> INSERT Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                          cfg.get<std::string>("db.mysql.files_table"),
                          cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL> ID of file, that was written: {}.", file_id);
            spdlog::debug("<MySQL, Perf> Executing INSERT Query took {} seconds.", query_sw);
            spdlog::debug("<MySQL, Perf> Writing file info in DB took {} seconds.", global_sw);
            delete pstmt;
            delete con;
            dbs++;
        }
        catch (sql::SQLException& e) {
            std::cout << "# ERR: SQLException in " << __FILE__;
            std::cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << std::endl;
            std::cout << "# ERR: " << e.what();
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }
    }

    spdlog::debug("<Perf> Writing file info in {} DB(s) took {} seconds.", dbs, total_sw);
}
