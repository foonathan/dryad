// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/optional_node.hpp>

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

struct container_node
: dryad::basic_node<node_kind::container, dryad::optional_node<node, leaf_node>>
{
    DRYAD_NODE_CTOR(container_node);
};
} // namespace

TEST_CASE("optional_node")
{
    dryad::tree<node_kind> tree;

    auto leaf      = tree.create<leaf_node>();
    auto container = tree.create<container_node>();
    CHECK(!container->has_child());
    CHECK(container->child() == nullptr);

    container->insert_child(leaf);
    CHECK(container->has_child());
    CHECK(container->child() == leaf);

    container->erase_child();
    CHECK(!container->has_child());
    CHECK(container->child() == nullptr);

    CHECK(container->replace_child(leaf) == nullptr);
    CHECK(container->has_child());
    CHECK(container->child() == leaf);

    auto new_leaf = tree.create<leaf_node>();
    CHECK(container->replace_child(new_leaf) == leaf);
    CHECK(container->has_child());
    CHECK(container->child() == new_leaf);
}

