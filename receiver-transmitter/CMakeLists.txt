project(House-Monitoring)
cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -D__BASEFILE__=\"$*\"")

set(RADIOHEADBASE "lib/RadioHead")

include_directories(include)
include_directories(${RADIOHEADBASE})
include_directories(${RADIOHEADBASE}/RHutil)

add_executable(receiver_transmitter 
    ${RADIOHEADBASE}/RHutil/RasPi.cpp
    ${RADIOHEADBASE}/RH_RF95.cpp
    ${RADIOHEADBASE}/RHDatagram.cpp
    ${RADIOHEADBASE}/RHHardwareSPI.cpp
    ${RADIOHEADBASE}/RHSPIDriver.cpp
    ${RADIOHEADBASE}/RHGenericDriver.cpp
    ${RADIOHEADBASE}/RHGenericSPI.cpp
    src/LiquidCrystal.cpp
    src/Print.cpp
    src/receiver_transmitter.cpp)

target_link_libraries(receiver_transmitter bcm2835)

add_executable(zmq_client
    src/zmq_client.cpp
)

target_link_libraries(zmq_client zmq)