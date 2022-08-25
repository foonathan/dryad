// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/tree.hpp>

#include <doctest/doctest.h>
#include <dryad/list_node.hpp>

namespace
{
enum class node_kind
{
    leaf,
    container,
};

using node = dryad::node<node_kind>;

struct leaf_node : dryad::basic_node<node_kind::leaf>
{
    DRYAD_NODE_CTOR(leaf_node);
};

struct container_node : dryad::basic_node<node_kind::container, dryad::list_node<node>>
{
    DRYAD_NODE_CTOR(container_node);
};
} // namespace

TEST_CASE("tree")
{
    dryad::tree<node_kind> tree;
    CHECK(tree.root() == nullptr);

    SUBCASE("basic")
    {
        auto a = tree.create<leaf_node>();
        auto b = tree.create<leaf_node>();
        auto c = tree.create<leaf_node>();

        auto container = tree.create<container_node>();
        container->insert_front(c);
        container->insert_front(b);
        container->insert_front(a);
        tree.set_root(container);
        CHECK(tree.root() == container);

        auto range = dryad::traverse(tree);
        auto iter  = range.begin();
        REQUIRE(iter != range.end());
        CHECK(iter->event == dryad::traverse_event::enter);
        CHECK(iter->node == container);

        ++iter;
        REQUIRE(iter != range.end());
        CHECK(iter->event == dryad::traverse_event::leaf);
        CHECK(iter->node == a);

        ++iter;
        REQUIRE(iter != range.end());
        CHECK(iter->event == dryad::traverse_event::leaf);
        CHECK(iter->node == b);

        ++iter;
        REQUIRE(iter != range.end());
        CHECK(iter->event == dryad::traverse_event::leaf);
        CHECK(iter->node == c);

        ++iter;
        REQUIRE(iter != range.end());
        CHECK(iter->event == dryad::traverse_event::exit);
        CHECK(iter->node == container);

        ++iter;
        CHECK(iter == range.end());
    }
}

TEST_CASE("visit")
{
    dryad::tree<node_kind> tree;
    CHECK(tree.root() == nullptr);

    auto a = tree.create<leaf_node>();
    auto b = tree.create<leaf_node>();
    auto c = tree.create<leaf_node>();

    auto container = tree.create<container_node>();
    container->insert_front(c);
    container->insert_front(b);
    container->insert_front(a);
    tree.set_root(container);
    CHECK(tree.root() == container);

    SUBCASE("with traverse_event")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_all(
            tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; },
            [&](dryad::traverse_event, container_node*) { ++container_count; });

        CHECK(leaf_count == 3);
        CHECK(container_count == 2);
    }
    SUBCASE("without traverse_event")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_all(
            tree, [&](leaf_node*) { ++leaf_count; }, [&](container_node*) { ++container_count; });

        CHECK(leaf_count == 3);
        CHECK(container_count == 1);
    }
    SUBCASE("catch all")
    {
        auto leaf_count = 0;
        auto node_count = 0;
        dryad::visit_all(
            tree, [&](dryad::traverse_event, node*) { ++node_count; },
            [&](dryad::traverse_event, leaf_node*) { ++leaf_count; });

        CHECK(leaf_count == 0);
        CHECK(node_count == 5);
    }
    SUBCASE("leaf only")
    {
        auto leaf_count = 0;
        dryad::visit(tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; });

        CHECK(leaf_count == 3);
    }
}

