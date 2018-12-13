cmake_minimum_required(VERSION 3.5)
project(Transmitter-House-Monitoring)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -D__BASEFILE__=\"$*\"")

set(RADIOHEADBASE "lib/RadioHead")
set(LIQUIDCRYSTALBASE "lib/LiquidCrystal")

include_directories(include)
include_directories(${RADIOHEADBASE})
include_directories(${RADIOHEADBASE}/RHutil)
include_directories(${LIQUIDCRYSTALBASE})

find_package(Boost 1.61.0 COMPONENTS system program_options REQUIRED)

add_executable(house_transmitter 
    ${RADIOHEADBASE}/RHutil/RasPi.cpp
    ${RADIOHEADBASE}/RH_RF95.cpp
    ${RADIOHEADBASE}/RHDatagram.cpp
    ${RADIOHEADBASE}/RHHardwareSPI.cpp
    ${RADIOHEADBASE}/RHSPIDriver.cpp
    ${RADIOHEADBASE}/RHGenericDriver.cpp
    ${RADIOHEADBASE}/RHGenericSPI.cpp
    ${LIQUIDCRYSTALBASE}/src/LiquidCrystal.cpp
    ${LIQUIDCRYSTALBASE}/src/Print.cpp
    src/house_transmitter.cpp)

target_include_directories(house_transmitter PRIVATE ${Boost_INCLUDE_DIR})

target_link_libraries(house_transmitter bcm2835)
target_link_libraries(house_transmitter ${Boost_LIBRARIES})
target_link_libraries(house_transmitter zmq)