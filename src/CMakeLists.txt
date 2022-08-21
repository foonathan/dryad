# Copyright (C) 022 Jonathan Müller and dryad contributors
# SPDX-License-Identifier: BSL-1.0

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/dryad ABSOLUTE)
set(header_files
        ${include_dir}/_detail/assert.hpp
        ${include_dir}/_detail/config.hpp
        ${include_dir}/_detail/iterator.hpp
        ${include_dir}/_detail/std.hpp

        ${include_dir}/list_node.hpp
        ${include_dir}/node.hpp
        ${include_dir}/tree.hpp)

add_library(dryad INTERFACE)
add_library(foonathan::dryad ALIAS dryad)
target_sources(dryad INTERFACE ${header_files})
target_compile_features(dryad INTERFACE cxx_std_17)
target_include_directories(dryad SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

