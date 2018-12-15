cmake_minimum_required(VERSION 3.5)
project(Webserver-House-Monitoring)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -g")
add_compile_options(-Wall -Wextra -Wsign-conversion)

include_directories("lib/simple-web-server")
include_directories("include")

find_package(Threads REQUIRED)
find_package(Boost 1.53.0 COMPONENTS system thread filesystem program_options REQUIRED)

add_library(simple-web-server INTERFACE)
target_include_directories(simple-web-server INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/Simple-Web-Server)
target_include_directories(simple-web-server INTERFACE ${Boost_INCLUDE_DIR})
target_link_libraries(simple-web-server INTERFACE ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(simple-web-server INTERFACE ${Boost_LIBRARIES})


add_executable(webserver src/webserver.cpp)
target_include_directories(webserver PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(webserver simple-web-server)
target_link_libraries(webserver ${Boost_LIBRARIES})
target_link_libraries(webserver sqlite3)
target_link_libraries(webserver zmq)