cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(ChaosMod VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
    # ensure unity iOS build doesn't re-include an unguarded header from Geode
    set(_fire_hdr_path "$ENV{GEODE_SDK}/loader/include/Geode/cocos/particle_nodes/firePngData.h")
    if(EXISTS "${_fire_hdr_path}")
        file(READ "${_fire_hdr_path}" _fire_hdr_content)
        if(NOT _fire_hdr_content MATCHES "#pragma once")
            file(WRITE "${_fire_hdr_path}" "#pragma once\n${_fire_hdr_content}")
            message(STATUS "Added pragma once guard to firePngData.h")
        endif()
    endif()
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
