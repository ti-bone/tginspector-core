/*
 * sql/checkFile.cpp
 * Core of TgInspector, namespace for working with DB, function for checking existence of file, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#include "sql.hpp"

bool Sql::checkFile(const std::string& file_id)
{
    int dbs = 0;
    spdlog::stopwatch total_sw;

    bool found = false;

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

            spdlog::debug("<PostgreSQL> Executing check file...");
            spdlog::debug("<PostgreSQL> Connected to a DB: {} [Server: {}, Port: {}].", pqconn.dbname(), pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Connection took {} seconds.", pqconn_sw);

            spdlog::stopwatch query_sw;

            // Init transaction
            pqxx::work transaction{pqconn};

            // Execute SQL query
            for (auto [file_path] : transaction.query<std::string>(
                    "SELECT file_path FROM " + cfg.get<std::string>("db.postgresql.files_table") +
                    " WHERE file_id='" + transaction.esc(file_id) + "'"))
            {
                if (fs::exists(file_path)) {
                    found = true;
                    break;
                }
            }

            spdlog::debug("<PostgreSQL> SELECT Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.files_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", query_sw);


        } catch (std::exception const &e) {
            std::cerr << e.what() << std::endl;
        }
        dbs++;
    }

    // mysql
    if (cfg.get<bool>("db.mysql.enabled") && !found) {
        try {
            spdlog::stopwatch global_sw;
            spdlog::stopwatch conn_sw;
            sql::mysql::MySQL_Driver *driver;
            sql::Connection *con;
            sql::PreparedStatement *pstmt;
            sql::ResultSet *res;

            // init mysql driver
            driver = sql::mysql::get_mysql_driver_instance();

            // connect to mysql instance
            con = driver->connect(cfg.get<std::string>("db.mysql.host"), cfg.get<std::string>("db.mysql.user"),
                                  cfg.get<std::string>("db.mysql.pass"));

            // set db
            con->setSchema(cfg.get<std::string>("db.mysql.database"));

            spdlog::debug("<MySQL> Executing check file...");
            spdlog::debug("<MySQL> Connected to a DB: {} [Server: {}].", con->getSchema().asStdString(), cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Connecting to DB took {} seconds.", conn_sw);

            spdlog::stopwatch query_sw;

            // read data from files table
            // prepare query
            pstmt = con->prepareStatement(
                    "SELECT file_name AS _path FROM " + cfg.get<std::string>("db.mysql.files_table") +
                    " WHERE file_id = ? ");

            // set values
            pstmt->setString(1, file_id);

            // execute query
            res = pstmt->executeQuery();

            while (res->next()) {
                std::string file_path = res->getString("_path");
                if (fs::exists(file_path)) {
                    found = true;
                    break;
                }
            }

            spdlog::debug("<MySQL> SELECT Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                          cfg.get<std::string>("db.mysql.files_table"),
                          cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL> ID of file, that was read: {}.", file_id);
            spdlog::debug("<MySQL, Perf> Executing SELECT Query took {} seconds.", query_sw);
            spdlog::debug("<MySQL, Perf> Checking file took {} seconds.", global_sw);
            delete pstmt;
            delete res;
            delete con;
            dbs++;
        }
        catch (sql::SQLException &e) {
            std::cout << "# ERR: SQLException in " << __FILE__;
            std::cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << std::endl;
            std::cout << "# ERR: " << e.what();
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }
    }

    spdlog::debug("<Perf> Checking file in {} DB(s) took {} seconds.", dbs, total_sw);

    return found;
}