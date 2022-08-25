// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/symbol.hpp>

#include <doctest/doctest.h>
#include <set>

TEST_CASE("symbol_interner")
{
    dryad::symbol_interner<void> symbols;

    SUBCASE("reserve")
    {
        symbols.reserve(10, 3);
    }
    SUBCASE("move assign")
    {
        symbols.intern("hello");
        symbols.intern("world");

        dryad::symbol_interner<void> other;
        other.reserve(10, 3);
        symbols = DRYAD_MOV(other);
    }

    auto abc1 = symbols.intern("abc");
    auto abc2 = symbols.intern("abc");
    CHECK(abc1 == abc2);
    CHECK(abc1.c_str(symbols) == doctest::String("abc"));

    SUBCASE("move construct")
    {
        auto other = DRYAD_MOV(symbols);
        symbols    = DRYAD_MOV(other);
    }

    auto def = symbols.intern("def");
    CHECK(def != abc1);
    CHECK(def.c_str(symbols) == doctest::String("def"));

    SUBCASE("many symbols interned")
    {
        std::set<decltype(symbols)::symbol> ids;
        for (auto i = 0u; i < 10 * 1024; ++i)
        {
            auto string = doctest::toString(std::uint64_t(-1) - i);
            auto id     = symbols.intern(string.c_str(), string.size());
            CHECK(ids.insert(id).second);
        }

        // Now insert everything again.
        for (auto i = 0u; i < 10 * 1024; ++i)
        {
            auto string = doctest::toString(std::uint64_t(-1) - i);
            auto id     = symbols.intern(string.c_str(), string.size());
            CHECK(!ids.insert(id).second);
        }
    }
}

