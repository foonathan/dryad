# Copyright (C) 2022 Jonathan Müller and dryad contributors
# SPDX-License-Identifier: BSL-1.0

set(tests
        detail/std.cpp
    
        abstract_node.cpp
        arena.cpp
        hash_forest.cpp
        node.cpp
        node_map.cpp
        tree.cpp
        symbol.cpp
        symbol_table.cpp)

add_executable(dryad_test ${tests})
target_link_libraries(dryad_test PRIVATE dryad_test_base)

add_test(NAME dryad_test COMMAND dryad_test)

