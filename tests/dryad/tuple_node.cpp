// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/tuple_node.hpp>

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

struct leaf_node : dryad::basic_node<node, node_kind::leaf>
{
    DRYAD_NODE_CTOR(leaf_node);
};

struct single_node : dryad::single_node<node, node_kind::container, leaf_node>
{
    explicit single_node(dryad::node_ctor ctor, leaf_node* node) : node_base(ctor, node) {}
};
} // namespace

TEST_CASE("single_node")
{
    dryad::tree<node_kind> tree;

    auto leaf      = tree.create<leaf_node>();
    auto container = tree.create<single_node>(leaf);
    CHECK(container->child() == leaf);

    auto new_leaf = tree.create<leaf_node>();
    CHECK(container->replace_child(new_leaf) == leaf);
    CHECK(container->child() == new_leaf);
}

namespace
{
struct array_node : dryad::array_node<node, node_kind::container, 2, leaf_node>
{
    explicit array_node(dryad::node_ctor ctor, leaf_node* a, leaf_node* b) : node_base(ctor, {a, b})
    {}
};
} // namespace

TEST_CASE("array_node")
{
    dryad::tree<node_kind> tree;

    auto leaf_a = tree.create<leaf_node>();
    auto leaf_b = tree.create<leaf_node>();

    auto container = tree.create<array_node>(leaf_a, leaf_b);
    CHECK(!container->children().empty());
    CHECK(container->children().size() == 2);
    CHECK(container->children()[0] == leaf_a);
    CHECK(container->children()[1] == leaf_b);
    CHECK(container->children().begin()[0] == leaf_a);
    CHECK(container->children().begin()[1] == leaf_b);

    auto new_leaf = tree.create<leaf_node>();
    CHECK(container->replace_child(1, new_leaf) == leaf_b);
    CHECK(container->children()[1] == new_leaf);
}

namespace
{
struct binary_node : dryad::binary_node<node, node_kind::container, leaf_node>
{
    explicit binary_node(dryad::node_ctor ctor, leaf_node* a, leaf_node* b) : node_base(ctor, a, b)
    {}
};
} // namespace

TEST_CASE("array_node")
{
    dryad::tree<node_kind> tree;

    auto leaf_a = tree.create<leaf_node>();
    auto leaf_b = tree.create<leaf_node>();

    auto container = tree.create<binary_node>(leaf_a, leaf_b);
    CHECK(container->left_child() == leaf_a);
    CHECK(container->right_child() == leaf_b);

    auto new_leaf = tree.create<leaf_node>();
    CHECK(container->replace_left_child(new_leaf) == leaf_a);
    CHECK(container->left_child() == new_leaf);

    CHECK(container->replace_right_child(leaf_a) == leaf_b);
    CHECK(container->right_child() == leaf_a);
}

