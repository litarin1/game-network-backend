cmake_minimum_required(VERSION 3.27)
project(game_networking
    LANGUAGES CXX
)
add_definitions(-DNETLIB_VERSION=${PROJECT_VERSION})


find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED IMPORTED_TARGET libuv)

add_library(net_lib STATIC net_lib.cpp)

#add_library(server_master STATIC master_server.cpp)
#add_library(server_lobby STATIC lobby.cpp)
### just controls godot server instances
#add_library(server_game STATIC game.cpp)

#add_library(client STATIC client.cpp)

#target_link_libraries(server_master PRIVATE net_lib)




add_executable(net_test net_test.cpp)
target_link_libraries(net_test PRIVATE net_lib)
target_include_directories(net_test PRIVATE ${LIBUV_INCLUDE_DIRS})
target_link_libraries(net_test PRIVATE PkgConfig::LIBUV)


find_package(OpenSSL REQUIRED)
add_executable(ssl_test ssl_test.cpp)
target_include_directories(ssl_test PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(ssl_test PRIVATE ${OPENSSL_LIBRARIES})




