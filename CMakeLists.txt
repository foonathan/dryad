# Copyright (C) 2022 Jonathan Müller and dryad contributors
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.8)
project(dryad LANGUAGES CXX)

add_subdirectory(src)

add_executable(dryad_prototype main.cpp)
target_link_libraries(dryad_prototype PRIVATE foonathan::dryad)

