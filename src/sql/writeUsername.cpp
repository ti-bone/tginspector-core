/*
 * sql/writeUsername.cpp
 * Core of TgInspector, namespace for working with DB, function for writing username->userid relation in DB, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#include "sql.hpp"

void Sql::writeUserName(const std::string& sender_id, const std::string& username)
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

            spdlog::debug("<PostgreSQL> Executing write username...");
            spdlog::debug("<PostgreSQL> Connected to a DB: {} [Server: {}, Port: {}].", pqconn.dbname(), pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Connection took {} seconds.", pqconn_sw);

            spdlog::stopwatch del_query_sw;

            // Init transaction
            pqxx::work transaction{pqconn};

            // Execute SQL query
            transaction.exec("delete from " + cfg.get<std::string>("db.postgresql.usernames_table") +
                             " where userid=" + transaction.esc(sender_id) + "or username='" + transaction.esc(username) + "'");

            spdlog::debug("<PostgreSQL> DELETE Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.usernames_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", del_query_sw);

            spdlog::stopwatch ins_query_sw;

            // Execute SQL query
            transaction.exec("insert into " + cfg.get<std::string>("db.postgresql.usernames_table") +
                             " values (" + transaction.esc(sender_id) + ", '" + transaction.esc(username) + "')");

            spdlog::debug("<PostgreSQL> INSERT Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.usernames_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", ins_query_sw);
            spdlog::debug("<PostgreSQL> Committing transaction...");

            // Commit transaction
            transaction.commit();

            spdlog::debug("<PostgreSQL> Transaction committed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.usernames_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL> ID of user, which username was written: {}.", sender_id);
            spdlog::debug("<PostgreSQL, Perf> Writing username in DB took {} seconds.", global_sw);
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
            sql::mysql::MySQL_Driver* driver;
            sql::Connection* con;
            sql::PreparedStatement* pstmt;

            // init mysql driver
            driver = sql::mysql::get_mysql_driver_instance();

            // connect to mysql instance
            con = driver->connect(cfg.get<std::string>("db.mysql.host"), cfg.get<std::string>("db.mysql.user"), cfg.get<std::string>("db.mysql.pass"));

            // set db
            con->setSchema(cfg.get<std::string>("db.mysql.database"));

            spdlog::debug("<MySQL> Executing write username...");
            spdlog::debug("<MySQL> Connected to a DB: {} [Server: {}].", con->getSchema().asStdString(), cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Connecting to DB took {} seconds.", conn_sw);

            spdlog::stopwatch del_query_sw;

            // delete userid->username ref from db if it exists
            // prepare query
            pstmt = con->prepareStatement("delete from " + cfg.get<std::string>("db.mysql.usernames_table") +
                                          " where userid=? or username=?");

            // set values
            pstmt->setString(1, sender_id);
            pstmt->setString(2, username);

            // execute query
            pstmt->execute();

            spdlog::debug("<MySQL> DELETE Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                          cfg.get<std::string>("db.mysql.usernames_table"),
                          cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Executing DELETE Query took {} seconds.", del_query_sw);

            delete pstmt;

            spdlog::stopwatch ins_query_sw;

            // write data in usernames table
            // prepare query
            pstmt = con->prepareStatement("INSERT INTO " + cfg.get<std::string>("db.mysql.usernames_table") +
                                          " (`userid`, `username`) VALUES (?, ?) ");

            // set values
            pstmt->setString(1, sender_id);
            pstmt->setString(2, username);

            // execute query
            pstmt->execute();

            spdlog::debug("<MySQL> INSERT Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                          cfg.get<std::string>("db.mysql.usernames_table"),
                          cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL> ID of user, which username was written: {}.", sender_id);
            spdlog::debug("<MySQL, Perf> Executing INSERT Query took {} seconds.", ins_query_sw);
            spdlog::debug("<MySQL, Perf> Writing username in DB took {} seconds.", global_sw);
            delete pstmt;
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

    spdlog::debug("<Perf> Writing username in {} DB(s) took {} seconds.", dbs, total_sw);
}
