// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>

#include <dryad/abstract_node.hpp>
#include <dryad/node.hpp>
#include <dryad/optional_node.hpp>
#include <dryad/tree.hpp>
#include <dryad/tuple_node.hpp>

enum class node_kind
{
    container,
    leaf
};

struct node : dryad::abstract_node_all<node_kind>
{
    DRYAD_ATTRIBUTE_USER_DATA16(short, foo);

    DRYAD_ABSTRACT_NODE_CTOR(node)
};

struct leaf_node : dryad::basic_node<node, node_kind::leaf>
{
    DRYAD_ATTRIBUTE_USER_DATA_PTR(const char*, msg);

    leaf_node(dryad::node_ctor ctor, const char* msg) : node_base(ctor)
    {
        set_msg(msg);
    }
};

struct container_node : dryad::binary_node<dryad::node<node_kind>, node_kind::container, leaf_node>
{
    container_node(dryad::node_ctor ctor, leaf_node* a, leaf_node* b) : node_base(ctor, a, b) {}
};

int main()
{
    dryad::tree<node_kind> tree;

    auto a         = tree.create<leaf_node>("a");
    auto b         = tree.create<leaf_node>("b");
    auto c         = tree.create<leaf_node>("c");
    auto container = tree.create<container_node>(a, b);
    tree.set_root(container);

    dryad::visit_all(
        tree, //
        [](container_node*) { std::puts("container"); },
        [](leaf_node* node) { std::puts(node->msg()); });
}

