cmake_minimum_required(VERSION 3.9.4 FATAL_ERROR)

project(tginspector-core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")

#set("Td_DIR" "D:\\td\\td\\tdlib\\lib\\cmake\\Td\\")
#set("Td_DIR" "/home/dev/work/td/build/")
find_package(Td 1.8.5 REQUIRED)

set (Boost_DETAILED_FAILURE_MSG ON)

#set("Boost_INCLUDE_DIR"   "W:\\boost_1_80_0\\")

find_package(Boost 1.74.0 REQUIRED)

#set(FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR "C:\\Program Files (x86)\\MySQL\\MySQL Connector C++ 8.0")
#include_directories(${FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR}/include)
#link_directories(${FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR}/lib/vs14)

include_directories("/home/dev/work/tginspector-core/deps/mysqlconn/include")
link_directories("/home/dev/work/tginspector-core/deps/mysqlconn/lib64")

#find_path(LIBPQXX_INCLUDE_DIR pqxx)
#find_library(LIBPQXX_LIBRARY libpqxx.a)
#find_library(LIBPQ_LIBRARY libpq.a)
#include_directories(${LIBPQXX_INCLUDE_DIR})

find_package(spdlog REQUIRED)

add_subdirectory(/home/dev/work/tginspector-core/deps/libpqxx build-pqxx)

file(GLOB sql "src/sql/*.hpp"
              "src/sql/*.cpp")

file(GLOB config "src/config/*.hpp"
                 "src/config/*.cpp")

add_executable(tginspector-core src/main.cpp ${sql} ${config})
target_link_libraries(tginspector-core PRIVATE Td::TdStatic Boost::boost mysqlcppconn pqxx spdlog::spdlog -static-libstdc++)
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")

set_property(TARGET tginspector-core PROPERTY CXX_STANDARD 17)
set_property(TARGET tginspector-core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

