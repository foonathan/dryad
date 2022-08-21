// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>

#include <dryad/node.hpp>
#include <dryad/node_list.hpp>
#include <dryad/tree.hpp>

enum class node_kind
{
    container,
    leaf
};

struct leaf_node : dryad::node<node_kind>
{
    const char* msg;

    leaf_node(dryad::node_ctor ctor, const char* msg) : base_node(ctor, node_kind::leaf), msg(msg)
    {}
};

struct container_node : dryad::node_list<node_kind>
{
    container_node(dryad::node_ctor ctor) : base_node(ctor, node_kind::container) {}
};

int main()
{
    dryad::tree<node_kind> tree;

    auto a         = tree.create<leaf_node>("a");
    auto b         = tree.create<leaf_node>("b");
    auto container = tree.create<container_node>();
    container->insert_front(b);
    container->insert_front(a);
    tree.set_root(container);

    for (auto child : container->children())
        std::puts(static_cast<leaf_node*>(child)->msg);
}

