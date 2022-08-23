// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>

#include <dryad/list_node.hpp>
#include <dryad/node.hpp>
#include <dryad/tree.hpp>

enum class node_kind
{
    container,
    leaf
};

struct leaf_node : dryad::basic_node<node_kind::leaf>
{
    DRYAD_ATTRIBUTE_USER_DATA_PTR(const char*, msg);

    leaf_node(dryad::node_ctor ctor, const char* msg) : node_base(ctor)
    {
        set_msg(msg);
    }
};

struct container_node : dryad::list_node<node_kind::container>
{
    DRYAD_NODE_CTOR(container_node);
};

int main()
{
    dryad::tree<node_kind> tree;

    auto a         = tree.create<leaf_node>("a");
    auto b         = tree.create<leaf_node>("b");
    auto c         = tree.create<leaf_node>("c");
    auto container = tree.create<container_node>();
    container->insert_front(c);
    container->insert_front(b);
    auto a_iter = container->insert_front(a);
    container->erase_after(a_iter);
    tree.set_root(container);

    for (auto node : container->children())
    {}

    dryad::visit_all(
        tree, //
        [](container_node*) { std::puts("container"); },
        [](leaf_node* node) { std::puts(node->msg()); });
}

