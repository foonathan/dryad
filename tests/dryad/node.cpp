// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/node.hpp>

#include <doctest/doctest.h>
#include <dryad/list_node.hpp>
#include <dryad/tree.hpp>
#include <iterator>

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

struct container_node : dryad::list_node<node, node_kind::container>
{
    DRYAD_NODE_CTOR(container_node);
};
} // namespace

TEST_CASE("node")
{
    auto tree = [&] {
        dryad::tree<node_kind> tree;

        auto a = tree.create<leaf_node>();
        auto b = tree.create<leaf_node>();
        auto c = tree.create<leaf_node>();

        auto container = tree.create<container_node>();
        container->insert_front(c);
        container->insert_front(b);
        container->insert_front(a);
        tree.set_root(container);

        return tree;
    }();

    auto node = tree.root();
    CHECK(node->is_linked_in_tree());
    CHECK(node->parent() == node);
    CHECK(node->siblings().empty());
    CHECK(node->is_container());

    node->set_color(dryad::color::black);
    CHECK(node->color() == dryad::color::black);

    for (auto child : dryad::node_cast<container_node>(node)->children())
    {
        CHECK(child->parent() == node);

        auto siblings = child->siblings();
        auto dist     = std::distance(siblings.begin(), siblings.end());
        CHECK(dist == 2);
    }
}

