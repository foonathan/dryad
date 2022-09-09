// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/tree.hpp>

#include <doctest/doctest.h>
#include <iterator>

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

struct container_node : dryad::basic_node<node_kind::container, dryad::container_node<node>>
{
    DRYAD_NODE_CTOR(container_node);

    void insert_front(node* n)
    {
        this->insert_child_after(nullptr, n);
    }
};
} // namespace

TEST_CASE("tree")
{
    dryad::tree<container_node> tree;
    CHECK(!tree.has_root());
    CHECK(tree.root() == nullptr);

    SUBCASE("clear")
    {
        tree.clear();
    }

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

TEST_CASE("forest")
{
    dryad::forest<container_node> forest;
    CHECK(forest.roots().empty());

    SUBCASE("clear")
    {
        forest.clear();
    }

    auto a = forest.create<leaf_node>();
    auto b = forest.create<leaf_node>();
    auto c = forest.create<leaf_node>();
    auto d = forest.create<leaf_node>();

    auto root1 = forest.create<container_node>();
    root1->insert_front(c);
    root1->insert_front(b);
    root1->insert_front(a);
    forest.insert_root(root1);

    CHECK(!forest.roots().empty());
    CHECK(forest.roots().front() == root1);
    CHECK(root1->parent() == root1);

    auto root2 = forest.create<container_node>();
    root2->insert_front(d);
    forest.insert_root_list(root2);

    CHECK(!forest.roots().empty());
    CHECK(forest.roots().front() == root1);
    CHECK(*std::next(forest.roots().begin()) == root2);
    CHECK(root1->parent() == root1);
    CHECK(root2->parent() == root2);

    dryad::visit_tree(root1, [&](node* ptr) { CHECK(ptr != d); });
    dryad::visit_tree(root2, [&](node* ptr) { CHECK((ptr == root2 || ptr == d)); });
}

TEST_CASE("visit_tree")
{
    dryad::tree<node> tree;
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
        dryad::visit_tree(
            tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; },
            [&](dryad::traverse_event, container_node*) { ++container_count; });

        CHECK(leaf_count == 3);
        CHECK(container_count == 2);
    }
    SUBCASE("with traverse_event_enter")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event_enter, leaf_node*) { ++leaf_count; },
            [&](dryad::traverse_event_enter, container_node*) { ++container_count; });

        CHECK(leaf_count == 0);
        CHECK(container_count == 1);
    }
    SUBCASE("with traverse_event_exit")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event_exit, leaf_node*) { ++leaf_count; },
            [&](dryad::traverse_event_exit, container_node*) { ++container_count; });

        CHECK(leaf_count == 0);
        CHECK(container_count == 1);
    }
    SUBCASE("child visitor")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; },
            [&](dryad::child_visitor<node_kind> visitor, container_node* node) {
                ++container_count;
                visitor(node->children().front());
            });

        CHECK(leaf_count == 1);
        CHECK(container_count == 1);
    }
    SUBCASE("no event argument")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_tree(
            tree, [&](leaf_node*) { ++leaf_count; }, [&](container_node*) { ++container_count; });

        CHECK(leaf_count == 3);
        CHECK(container_count == 1);
    }
    SUBCASE("ignore_node")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; },
            dryad::ignore_node<container_node>);

        CHECK(leaf_count == 0);
        CHECK(container_count == 0);
    }
    SUBCASE("catch all first")
    {
        auto leaf_count = 0;
        auto node_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event, node*) { ++node_count; },
            [&](dryad::traverse_event, leaf_node*) { ++leaf_count; });

        CHECK(leaf_count == 3);
        CHECK(node_count == 5);
    }
    SUBCASE("catch all last")
    {
        auto leaf_count = 0;
        auto node_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; },
            [&](dryad::traverse_event, node*) { ++node_count; });

        CHECK(leaf_count == 3);
        CHECK(node_count == 2);
    }
    SUBCASE("leaf only")
    {
        auto leaf_count = 0;
        dryad::visit_tree(tree, [&](dryad::traverse_event, leaf_node*) { ++leaf_count; });

        CHECK(leaf_count == 3);
    }
    SUBCASE("mutable")
    {
        auto leaf_count      = 0;
        auto container_count = 0;
        dryad::visit_tree(
            tree, [&](dryad::traverse_event, leaf_node*) mutable { ++leaf_count; },
            [&](dryad::traverse_event, container_node*) mutable { ++container_count; });

        CHECK(leaf_count == 3);
        CHECK(container_count == 2);
    }
}

