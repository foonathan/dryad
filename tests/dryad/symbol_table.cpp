// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/symbol_table.hpp>

#include <doctest/doctest.h>
#include <set>

namespace
{
template <typename Table, typename... T>
void check_iter(const Table& table, T... elements)
{
    std::set<typename Table::symbol> set;
    for (auto [sym, decl] : table)
        set.insert(sym);

    CHECK(set.size() == sizeof...(T));
    auto has_elements = ((set.count(elements) == 1) && ...);
    CHECK(has_elements);
}
} // namespace

TEST_CASE("symbol_table")
{
    dryad::symbol_interner<void> symbols;
    auto                         a = symbols.intern("a");
    auto                         b = symbols.intern("b");
    auto                         c = symbols.intern("c");

    dryad::symbol_table<decltype(a), decltype(a)*> table;
    CHECK(table.empty());
    CHECK(table.size() == 0);
    CHECK(table.capacity() == 0);
    CHECK(table.begin() == table.end());

    SUBCASE("initial rehash")
    {
        table.rehash(10);
        CHECK(table.empty());
        CHECK(table.size() == 0);
        CHECK(table.capacity() >= 10);
        CHECK(table.begin() == table.end());
    }
    SUBCASE("move")
    {
        decltype(table) other;
        table = DRYAD_MOV(other);
    }
    CHECK(!table.lookup(a));
    CHECK(!table.lookup(b));
    CHECK(!table.lookup(c));

    CHECK(table.insert_or_shadow(a, &a) == nullptr);
    CHECK(table.insert_or_shadow(b, &b) == nullptr);
    CHECK(table.insert_or_shadow(c, &c) == nullptr);
    CHECK(!table.empty());
    CHECK(table.size() == 3);
    CHECK(table.lookup(a) == &a);
    CHECK(table.lookup(b) == &b);
    CHECK(table.lookup(c) == &c);
    check_iter(table, a, b, c);

    SUBCASE("rehash")
    {
        table.rehash(100);
        CHECK(!table.empty());
        CHECK(table.size() == 3);
        CHECK(table.capacity() >= 100);
    }
    SUBCASE("move")
    {
        decltype(table) other(DRYAD_MOV(table));
        table = DRYAD_MOV(other);
    }

    CHECK(table.insert_or_shadow(b, &a) == &b);
    CHECK(!table.empty());
    CHECK(table.size() == 3);
    CHECK(table.lookup(a) == &a);
    CHECK(table.lookup(b) == &a);
    CHECK(table.lookup(c) == &c);
    check_iter(table, a, b, c);

    SUBCASE("second rehash")
    {
        table.rehash(1000);
        CHECK(!table.empty());
        CHECK(table.size() == 3);
        CHECK(table.capacity() >= 1000);
    }

    CHECK(table.remove(b) == &a);
    CHECK(!table.empty());
    CHECK(table.size() == 2);
    CHECK(table.lookup(a) == &a);
    CHECK(table.lookup(b) == nullptr);
    CHECK(table.lookup(c) == &c);
    check_iter(table, a, c);
}

