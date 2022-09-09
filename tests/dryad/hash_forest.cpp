// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/hash_forest.hpp>

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

struct leaf_node : dryad::basic_node<node_kind::leaf>
{
    int data;

    leaf_node(dryad::node_ctor ctor, int i) : node_base(ctor), data(i) {}
};

struct container_node : dryad::basic_node<node_kind::container, dryad::container_node<node>>
{
    template <typename... Children>
    container_node(dryad::node_ctor ctor, Children... children) : node_base(ctor)
    {
        insert_children_after(nullptr, children...);
    }
};

struct node_hasher : dryad::node_hasher_base<node_hasher, leaf_node, container_node>
{
    template <typename HashAlgorithm>
    static void hash(HashAlgorithm& hasher, const leaf_node* n)
    {
        hasher.hash_scalar(n->data);
    }
    template <typename HashAlgorithm>
    static void hash(HashAlgorithm&, const container_node*)
    {}

    static bool is_equal(const leaf_node* lhs, const leaf_node* rhs)
    {
        return lhs->data == rhs->data;
    }
    static bool is_equal(const container_node*, const container_node*)
    {
        return true;
    }
};
} // namespace

TEST_CASE("hash_forest")
{
    dryad::hash_forest<container_node, node_hasher> forest;

    auto container1 = forest.create([](decltype(forest)::node_creator creator) {
        auto a = creator.create<leaf_node>(1);
        auto b = creator.create<leaf_node>(2);
        auto c = creator.create<leaf_node>(3);
        return creator.create<container_node>(a, b, c);
    });
    auto container2 = forest.create([](decltype(forest)::node_creator creator) {
        auto a = creator.create<leaf_node>(1);
        auto b = creator.create<leaf_node>(2);
        auto c = creator.create<leaf_node>(3);
        return creator.create<container_node>(a, b, c);
    });
    CHECK(container1 == container2);

    auto container3 = forest.create([](decltype(forest)::node_creator creator) {
        auto a = creator.create<leaf_node>(1);
        auto b = creator.create<leaf_node>(2);
        return creator.create<container_node>(a, b);
    });
    CHECK(container1 != container3);
}

