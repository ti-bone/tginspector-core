/*
 * sql/writeAvatar.cpp
 * Core of TgInspector, namespace for working with DB, function for writing user's profile pic into DB, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

#include "sql.hpp"

void Sql::writeAvatar(const std::string& file_id, const std::string& user_id, const std::string& timestamp)
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

            spdlog::debug("<PostgreSQL> Executing write avatar...");
            spdlog::debug("<PostgreSQL> Connected to a DB: {} [Server: {}, Port: {}].", pqconn.dbname(), pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Connection took {} seconds.", pqconn_sw);

            spdlog::stopwatch query_sw;

            // Init transaction
            pqxx::work transaction{pqconn};

            bool cont = true;

            // Execute SQL query
            for (auto [count] : transaction.query<int64_t>(
                    "SELECT COUNT(*) FROM " + cfg.get<std::string>("db.postgresql.avatars_table") +
                    " WHERE file_id='" + transaction.esc(file_id) + "' AND user_id=" + transaction.esc(user_id)))
            {
                spdlog::debug("<PostgreSQL> Count: {}", count);
                if (count >= 1) {
                    spdlog::debug("<PostgreSQL> This pic of this user already exists, stopping.");
                    cont = false;
                    break;
                }
            }

            spdlog::debug("<PostgreSQL> SELECT Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                          pqconn.dbname(), cfg.get<std::string>("db.postgresql.avatars_table"),
                          pqconn.hostname(), pqconn.port());
            spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", query_sw);

            if (cont) {
                spdlog::stopwatch insert_query_sw;

                transaction.exec("insert into " + cfg.get<std::string>("db.postgresql.avatars_table") + " "
                                        " values (" + transaction.esc(user_id) + ", "
                                        "'" + transaction.esc(file_id) + "', " + transaction.esc(timestamp) + ")");

                spdlog::debug("<PostgreSQL> INSERT Query executed in DB: {}, Table: {} [Server: {}, Port: {}].",
                              pqconn.dbname(), cfg.get<std::string>("db.postgresql.avatars_table"),
                              pqconn.hostname(), pqconn.port());
                spdlog::debug("<PostgreSQL, Perf> Executing query took {} seconds.", insert_query_sw);
                spdlog::debug("<PostgreSQL> Committing transaction...");

                // Commit transaction
                transaction.commit();

                spdlog::debug("<PostgreSQL> Transaction committed in DB: {}, Table: {} [Server: {}, Port: {}].",
                              pqconn.dbname(), cfg.get<std::string>("db.postgresql.avatars_table"),
                              pqconn.hostname(), pqconn.port());
                spdlog::debug("<PostgreSQL> ID of user, which avatar info was written into DB: {}.", user_id);
                spdlog::debug("<PostgreSQL, Perf> Writing avatar info in DB took {} seconds.", global_sw);
                dbs++;
            }
        } catch (std::exception const &e) {
            std::cerr << e.what() << std::endl;
        }
    }

    // mysql
    if (cfg.get<bool>("db.mysql.enabled")) {
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

            spdlog::debug("<MySQL> Executing write avatar...");
            spdlog::debug("<MySQL> Connected to a DB: {} [Server: {}].", con->getSchema().asStdString(), cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Connecting to DB took {} seconds.", conn_sw);

            spdlog::stopwatch query_sw;

            bool cont = true;

            // read data from avatars table
            // prepare query
            pstmt = con->prepareStatement(
                    "select COUNT(*) from " + cfg.get<std::string>("db.mysql.avatars_table") +
                    " where file_id=? and user_id=?");

            // set values
            pstmt->setString(1, file_id);
            pstmt->setString(2, user_id);

            // execute query
            res = pstmt->executeQuery();

            while (res->next()) {
                int count = res->getInt(1);
                spdlog::debug("<MySQL> Count: {}", count);
                if (count >= 1) {
                    spdlog::debug("<MySQL> This pic of this user already exists, skipping.");
                    cont = false;
                    break;
                }
            }

            spdlog::debug("<MySQL> SELECT Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                          cfg.get<std::string>("db.mysql.avatars_table"),
                          cfg.get<std::string>("db.mysql.host"));
            spdlog::debug("<MySQL, Perf> Executing SELECT Query took {} seconds.", query_sw);

            delete res;
            delete pstmt;

            if (cont) {
                spdlog::stopwatch insert_query_sw;

                // write data into avatars table
                // prepare query
                pstmt = con->prepareStatement(
                        "INSERT INTO " + cfg.get<std::string>("db.mysql.avatars_table") +
                        " (`user_id`, `file_id`, `timestamp`) VALUES (?, ?, ?) ");

                // set values
                pstmt->setString(1, user_id);
                pstmt->setString(2, file_id);
                pstmt->setString(3, timestamp);

                // exec query
                pstmt->execute();

                spdlog::debug("<MySQL> INSERT Query executed in DB: {}, Table: {} [Server: {}].", con->getSchema().asStdString(),
                              cfg.get<std::string>("db.mysql.avatars_table"),
                              cfg.get<std::string>("db.mysql.host"));
                spdlog::debug("<MySQL, Perf> Executing INSERT Query took {} seconds.", query_sw);
                spdlog::debug("<MySQL> ID of user, which avatar info was written into DB: {}.", user_id);
                spdlog::debug("<MySQL, Perf> Writing avatar took {} seconds.", global_sw);

                // cleanup that shit
                delete pstmt;

                // and increase dbs counter
                dbs++;
            }

            delete con;
        }
        catch (sql::SQLException &e) {
            std::cout << "# ERR: SQLException in " << __FILE__;
            std::cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << std::endl;
            std::cout << "# ERR: " << e.what();
            std::cout << " (MySQL error code: " << e.getErrorCode();
            std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        }
    }

    spdlog::debug("<Perf> Writing avatar in {} DB(s) took {} seconds.", dbs, total_sw);
}