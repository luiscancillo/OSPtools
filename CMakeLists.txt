cmake_minimum_required(VERSION 3.21)
project(OSPtools)
set(CMAKE_CXX_STANDARD 11)
include_directories(../CommonClasses)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS ${CMAKE_BUILD_TYPE})
    find_library(COMMON_CLASSES CommonClasses PATHS ../CommonClasses/cmake-build-debug)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    message(STATUS ${CMAKE_BUILD_TYPE})
    find_library(COMMON_CLASSES CommonClasses PATHS ../CommonClasses/cmake-build-release)
endif()

add_executable(GP2toOSP GP2toOSP.cpp)
target_link_libraries(GP2toOSP LINK_PUBLIC ${COMMON_CLASSES})
add_executable(OSPtoRINEX OSPtoRINEX.cpp)
target_link_libraries(OSPtoRINEX LINK_PUBLIC ${COMMON_CLASSES})
add_executable(OSPtoRTK OSPtoRTK.cpp)
target_link_libraries(OSPtoRTK LINK_PUBLIC ${COMMON_CLASSES})
add_executable(OSPtoTXT OSPtoTXT.cpp)
target_link_libraries(OSPtoTXT LINK_PUBLIC ${COMMON_CLASSES})
add_executable(PacketToOSP PacketToOSP.cpp)
target_link_libraries(PacketToOSP LINK_PUBLIC ${COMMON_CLASSES})
add_executable(RXtoOSP RXtoOSP.cpp)
target_link_libraries(RXtoOSP LINK_PUBLIC ${COMMON_CLASSES})
add_executable(SynchroRX SynchroRX.cpp)
target_link_libraries(SynchroRX LINK_PUBLIC ${COMMON_CLASSES})
