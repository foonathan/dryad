# Copyright (C) 2022 Jonathan Müller and dryad contributors
# SPDX-License-Identifier: BSL-1.0

#=== Fetch doctest ===#
message(STATUS "Fetching doctest")
include(FetchContent)
FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.8.zip)
FetchContent_MakeAvailable(doctest)

add_library(dryad_test_base ${CMAKE_CURRENT_SOURCE_DIR}/doctest_main.cpp)
target_link_libraries(dryad_test_base PUBLIC doctest)
target_include_directories(dryad_test_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_features(dryad_test_base PUBLIC cxx_std_17)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
        target_compile_options(dryad_test_base PUBLIC /WX /W3 /D _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(dryad_test_base PUBLIC -pedantic-errors -Werror -Wall -Wextra -Wconversion -Wsign-conversion)
    endif()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(dryad_test_base PUBLIC -pedantic-errors -Werror -Wall -Wextra -Wconversion -Wsign-conversion)
elseif(MSVC)
    target_compile_options(dryad_test_base PUBLIC /WX /W3 /D _CRT_SECURE_NO_WARNINGS)
endif()

#=== tests ===#
add_subdirectory(dryad)

