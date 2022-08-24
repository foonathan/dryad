// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/arena.hpp>

#include <cstring>
#include <doctest/doctest.h>

TEST_CASE("arena")
{
    dryad::arena<> arena;

    SUBCASE("basic")
    {
        auto i = arena.construct<int>(42);

        auto fill_block = arena.allocate(10 * 1024ull, 1);
        std::memset(fill_block, 'a', 10 * 1024ull);

        auto next_block = arena.allocate(10 * 1024ull, 1);
        std::memset(next_block, 'b', 10 * 1024ull);

        REQUIRE(*i == 42);

        for (auto i = 0u; i != 10 * 1024ull; ++i)
        {
            REQUIRE(static_cast<char*>(fill_block)[i] == 'a');
            REQUIRE(static_cast<char*>(next_block)[i] == 'b');
        }
    }
    SUBCASE("clear and re-use")
    {
        auto a1 = arena.allocate(10 * 1024ull, 1);
        std::memset(a1, 'a', 10 * 1024ull);

        auto a2 = arena.allocate(10 * 1024ull, 1);
        std::memset(a2, 'b', 10 * 1024ull);

        arena.clear();

        auto b1 = arena.allocate(10 * 1024ull, 1);
        std::memset(b1, 'A', 10 * 1024ull);

        auto b2 = arena.allocate(10 * 1024ull, 1);
        std::memset(b2, 'B', 10 * 1024ull);

        CHECK(a1 == b1);
        CHECK(a2 == b2);

        for (auto i = 0u; i != 10 * 1024ull; ++i)
        {
            REQUIRE(static_cast<char*>(b1)[i] == 'A');
            REQUIRE(static_cast<char*>(b2)[i] == 'B');
        }
    }
}

