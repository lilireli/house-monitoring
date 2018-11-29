cmake_minimum_required(VERSION 3.5)
project(Server_Serre)

# set(CMAKE_CXX_STANDARD 17)

# add_library(pistache)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -g")
add_compile_options(-Wall -Wextra -Wsign-conversion)

# add_executable(server src/main.cpp)

# target_link_libraries(server pistache)

include_directories("lib/Simple-Web-Server")
include_directories("include")

add_executable(zmq_client
    src/zmq_client.cpp
)

target_link_libraries(zmq_client zmq)

add_executable(communicator
    src/communicator.cpp
)

target_link_libraries(communicator zmq)
target_link_libraries(communicator sqlite3)



add_library(simple-web-server INTERFACE)

target_include_directories(simple-web-server INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/Simple-Web-Server)

find_package(Threads REQUIRED)
target_link_libraries(simple-web-server INTERFACE ${CMAKE_THREAD_LIBS_INIT})


find_package(Boost 1.53.0 COMPONENTS system thread filesystem REQUIRED)

target_link_libraries(simple-web-server INTERFACE ${Boost_LIBRARIES})
target_include_directories(simple-web-server INTERFACE ${Boost_INCLUDE_DIR})

add_executable(webserver src/webserver.cpp)
target_link_libraries(webserver simple-web-server)
target_link_libraries(webserver ${Boost_LIBRARIES})
target_include_directories(webserver PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(webserver sqlite3)
target_link_libraries(webserver zmq)