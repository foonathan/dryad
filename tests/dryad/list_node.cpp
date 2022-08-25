// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/list_node.hpp>

#include <doctest/doctest.h>
#include <dryad/tree.hpp>

namespace
{
enum class node_kind
{
    leaf,
    container,
};

using node = dryad::node<node_kind>;

struct leaf_node : dryad::basic_node<node_kind::leaf, node>
{
    DRYAD_NODE_CTOR(leaf_node);
};

struct container_node : dryad::basic_node<node_kind::container, dryad::list_node<node, leaf_node>>
{
    DRYAD_NODE_CTOR(container_node);
};

void check_children(const container_node* node, leaf_node* a, leaf_node* b, leaf_node* c)
{
    auto children = node->children();
    auto iter     = children.begin();

    auto count = 0;
    if (a != nullptr)
    {
        REQUIRE(iter != children.end());
        CHECK(iter == a);
        ++iter;
        ++count;
    }
    if (b != nullptr)
    {
        REQUIRE(iter != children.end());
        CHECK(iter == b);
        ++iter;
        ++count;
    }
    if (c != nullptr)
    {
        REQUIRE(iter != children.end());
        CHECK(iter == c);
        ++iter;
        ++count;
    }

    CHECK(children.size() == count);
    if (count > 0)
        CHECK(!children.empty());
}
} // namespace

TEST_CASE("list_node")
{
    dryad::tree<node_kind> tree;

    auto a = tree.create<leaf_node>();
    auto b = tree.create<leaf_node>();
    auto c = tree.create<leaf_node>();

    auto container = tree.create<container_node>();
    check_children(container, nullptr, nullptr, nullptr);

    container->insert_front(c);
    check_children(container, c, nullptr, nullptr);
    auto pos = container->insert_front(b);
    check_children(container, b, c, nullptr);
    container->insert_front(a);
    check_children(container, a, b, c);

    CHECK(container->erase_front() == a);
    check_children(container, b, c, nullptr);

    container->insert_after(pos, a);
    check_children(container, b, a, c);

    CHECK(container->erase_after(pos) == a);
    check_children(container, b, c, nullptr);
}

