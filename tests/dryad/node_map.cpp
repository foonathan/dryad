// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/node_map.hpp>

#include <doctest/doctest.h>
#include <dryad/tree.hpp>
#include <string>

namespace
{
struct node : dryad::basic_node<'a'>
{
    DRYAD_NODE_CTOR(node);
};

void check_entry(dryad::node_map<node, std::string>& map, node* key, const char* str)
{
    REQUIRE(map.contains(key));
    CHECK(*map.lookup(key) == str);

    auto entry = map.lookup_entry(key);
    CHECK(entry);
    CHECK(entry.node() == key);
    CHECK(entry.value() == str);
}
} // namespace

TEST_CASE("node_map")
{
    dryad::tree<char> tree;
    auto              a = tree.create<node>();
    auto              b = tree.create<node>();
    auto              c = tree.create<node>();
    auto              d = tree.create<node>();

    dryad::node_map<node, std::string> map;
    CHECK(map.empty());
    CHECK(map.size() == 0);
    CHECK(map.capacity() == 0);

    SUBCASE("initial rehash")
    {
        map.rehash(10);
        CHECK(map.empty());
        CHECK(map.size() == 0);
        CHECK(map.capacity() >= 10);
    }
    SUBCASE("move")
    {
        decltype(map) other;
        map = DRYAD_MOV(other);
    }
    CHECK(!map.contains(a));
    CHECK(!map.contains(b));
    CHECK(!map.contains(c));

    CHECK(map.insert(a, "a"));
    CHECK(map.insert(b, "b"));
    CHECK(map.insert(c, "c"));
    check_entry(map, a, "a");
    check_entry(map, b, "b");
    check_entry(map, c, "c");

    SUBCASE("rehash")
    {
        map.rehash(100);
        CHECK(!map.empty());
        CHECK(map.size() == 3);
        CHECK(map.capacity() >= 100);
    }
    SUBCASE("move")
    {
        decltype(map) other(DRYAD_MOV(map));
        map = DRYAD_MOV(other);
    }

    CHECK(map.insert_or_update(d, "d"));
    CHECK(!map.insert_or_update(c, "C"));
    check_entry(map, a, "a");
    check_entry(map, b, "b");
    check_entry(map, c, "C");
    check_entry(map, d, "d");

    SUBCASE("second rehash")
    {
        map.rehash(1000);
        CHECK(!map.empty());
        CHECK(map.size() == 4);
        CHECK(map.capacity() >= 1000);
    }

    CHECK(map.remove(d));
    CHECK(!map.contains(d));
}

namespace
{

void check_entry(dryad::node_set<node>& set, node* key)
{
    REQUIRE(set.contains(key));

    auto entry = set.lookup_entry(key);
    CHECK(entry);
    CHECK(entry.node() == key);
}
} // namespace

TEST_CASE("node_set")
{
    dryad::tree<char> tree;
    auto              a = tree.create<node>();
    auto              b = tree.create<node>();
    auto              c = tree.create<node>();

    dryad::node_set<node> set;
    CHECK(set.empty());
    CHECK(set.size() == 0);
    CHECK(set.capacity() == 0);

    SUBCASE("initial rehash")
    {
        set.rehash(10);
        CHECK(set.empty());
        CHECK(set.size() == 0);
        CHECK(set.capacity() >= 10);
    }
    SUBCASE("move")
    {
        decltype(set) other;
        set = DRYAD_MOV(other);
    }

    CHECK(set.insert(a));
    CHECK(set.insert(b));
    CHECK(set.insert(c));
    check_entry(set, a);
    check_entry(set, b);
    check_entry(set, c);

    SUBCASE("rehash")
    {
        set.rehash(100);
        CHECK(!set.empty());
        CHECK(set.size() == 3);
        CHECK(set.capacity() >= 100);
    }
    SUBCASE("move")
    {
        decltype(set) other(DRYAD_MOV(set));
        set = DRYAD_MOV(other);
    }

    CHECK(set.remove(c));
    CHECK(!set.contains(c));
}

