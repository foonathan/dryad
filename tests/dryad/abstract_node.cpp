// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/abstract_node.hpp>

#include <doctest/doctest.h>
#include <dryad/tree.hpp>

namespace abstract_node
{
enum class node_kind
{
    leaf1,
    leaf2,
};

using node = dryad::node<node_kind>;

struct base_node : dryad::abstract_node<node, node_kind::leaf1>
{
    DRYAD_ATTRIBUTE_USER_DATA16(std::uint16_t, foo);

    DRYAD_ABSTRACT_NODE_CTOR(base_node);
};

struct leaf_node1 : dryad::basic_node<node_kind::leaf1, base_node>
{
    DRYAD_NODE_CTOR(leaf_node1);
};
struct leaf_node2 : dryad::basic_node<node_kind::leaf2>
{
    DRYAD_NODE_CTOR(leaf_node2);
};
} // namespace abstract_node

TEST_CASE("abstract_node")
{
    using namespace abstract_node;
    dryad::tree<node> tree;

    auto leaf1 = tree.create<leaf_node1>();
    leaf1->set_foo(11);
    CHECK(leaf1->foo() == 11);
    CHECK(dryad::node_has_kind<base_node>(leaf1));

    auto leaf2 = tree.create<leaf_node2>();
    CHECK(!dryad::node_has_kind<base_node>(leaf2));
}

namespace abstract_node_range
{
enum class node_kind
{
    leaf1,
    leaf2,
};

using node = dryad::node<node_kind>;

struct base_node : dryad::abstract_node_range<node, node_kind::leaf1, node_kind::leaf1>
{
    DRYAD_ATTRIBUTE_USER_DATA16(std::uint16_t, foo);

    DRYAD_ABSTRACT_NODE_CTOR(base_node);
};

struct leaf_node1 : dryad::basic_node<node_kind::leaf1, base_node>
{
    DRYAD_NODE_CTOR(leaf_node1);
};
struct leaf_node2 : dryad::basic_node<node_kind::leaf2>
{
    DRYAD_NODE_CTOR(leaf_node2);
};
} // namespace abstract_node_range

TEST_CASE("abstract_node_range")
{
    using namespace abstract_node_range;
    dryad::tree<node> tree;

    auto leaf1 = tree.create<leaf_node1>();
    leaf1->set_foo(11);
    CHECK(leaf1->foo() == 11);
    CHECK(dryad::node_has_kind<base_node>(leaf1));

    auto leaf2 = tree.create<leaf_node2>();
    CHECK(!dryad::node_has_kind<base_node>(leaf2));
}

namespace abstract_node_all
{
enum class node_kind
{
    leaf1,
    leaf2,
};

struct node : dryad::abstract_node_all<node_kind>
{
    DRYAD_ATTRIBUTE_USER_DATA16(std::uint16_t, foo);

    DRYAD_ABSTRACT_NODE_CTOR(node);
};

struct leaf_node1 : dryad::basic_node<node_kind::leaf1, node>
{
    DRYAD_NODE_CTOR(leaf_node1);
};
struct leaf_node2 : dryad::basic_node<node_kind::leaf2, node>
{
    DRYAD_NODE_CTOR(leaf_node2);
};
} // namespace abstract_node_all

TEST_CASE("abstract_node_all")
{
    using namespace abstract_node_all;
    dryad::tree<node> tree;

    auto leaf1 = tree.create<leaf_node1>();
    leaf1->set_foo(11);
    CHECK(leaf1->foo() == 11);
    CHECK(dryad::node_has_kind<node>(leaf1));

    auto leaf2 = tree.create<leaf_node2>();
    leaf2->set_foo(11);
    CHECK(leaf2->foo() == 11);
    CHECK(dryad::node_has_kind<node>(leaf2));
}

