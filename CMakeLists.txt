# Copyright (C) 2022 Jonathan Müller and dryad contributors
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.8)
project(dryad LANGUAGES CXX)

add_subdirectory(src)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    option(DRYAD_BUILD_TESTS "whether or not tests should be built" ON)

    if(DRYAD_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

add_executable(dryad_prototype main.cpp)
target_link_libraries(dryad_prototype PRIVATE foonathan::dryad)

