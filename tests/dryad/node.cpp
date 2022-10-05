// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/node.hpp>

#include <doctest/doctest.h>
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

struct leaf_node : dryad::basic_node<node_kind::leaf>
{
    DRYAD_NODE_CTOR(leaf_node);
};

struct container_node : dryad::basic_node<node_kind::container, dryad::container_node<node>>
{
    DRYAD_NODE_CTOR(container_node);

    template <typename... Children>
    void insert_front(Children*... ns)
    {
        this->insert_children_after(nullptr, ns...);
    }
    void insert_front(dryad::unlinked_node_list<node> list)
    {
        this->insert_child_list_after(nullptr, list);
        this->insert_child_list_after(nullptr, dryad::unlinked_node_list<node>());
    }

    auto children() const
    {
        auto node_children = node_base::children();
        return dryad::make_node_range<leaf_node>(node_children.begin(), node_children.end());
    }

    DRYAD_CHILD_NODE_GETTER(leaf_node, first, nullptr)
    DRYAD_CHILD_NODE_GETTER(leaf_node, second, first())
    DRYAD_CHILD_NODE_GETTER(leaf_node, third, second())
};
} // namespace

TEST_CASE("node")
{
    dryad::tree<node> tree;

    auto a = tree.create<leaf_node>();
    auto b = tree.create<leaf_node>();
    auto c = tree.create<leaf_node>();

    auto container = tree.create<container_node>();
    SUBCASE("variadic insert")
    {
        container->insert_front(a, b, c);
    }
    SUBCASE("list insert")
    {
        dryad::unlinked_node_list<node> list;
        CHECK(list.empty());
        CHECK(list.begin() == list.end());

        list.push_back(b);
        CHECK(!list.empty());
        CHECK(list.has_single_element());
        CHECK(list.front() == b);
        CHECK(list.back() == b);

        list.append(c);
        CHECK(!list.empty());
        CHECK(!list.has_single_element());
        CHECK(list.front() == b);
        CHECK(list.back() == c);

        list.push_front(a);
        CHECK(!list.empty());
        CHECK(!list.has_single_element());
        CHECK(list.front() == a);
        CHECK(list.back() == c);

        CHECK(list.pop_front() == a);
        CHECK(!list.empty());
        CHECK(!list.has_single_element());
        CHECK(list.front() == b);
        CHECK(list.back() == c);

        list.push_front(a);

        auto iter = list.begin();
        CHECK(iter != list.end());
        CHECK(*iter == a);
        ++iter;
        CHECK(iter != list.end());
        CHECK(*iter == b);
        ++iter;
        CHECK(iter != list.end());
        CHECK(*iter == c);
        ++iter;
        CHECK(iter == list.end());

        container->insert_front(list);
    }
    tree.set_root(container);

    CHECK(container->first() == a);
    CHECK(container->second() == b);
    CHECK(container->third() == c);

    auto node = tree.root();
    CHECK(node->is_linked_in_tree());
    CHECK(node->parent() == node);
    CHECK(node->siblings().empty());

    node->set_color(dryad::color::black);
    CHECK(node->color() == dryad::color::black);

    for (auto child : node->children())
    {
        CHECK(child->parent() == node);

        auto siblings = child->siblings();
        auto dist     = std::distance(siblings.begin(), siblings.end());
        CHECK(dist == 2);

        CHECK(child->children().empty());
    }
    for (const leaf_node* child : dryad::node_cast<container_node>(node)->children())
    {
        CHECK(child->parent() == node);

        auto siblings = child->siblings();
        auto dist     = std::distance(siblings.begin(), siblings.end());
        CHECK(dist == 2);

        CHECK(child->children().empty());
    }
}

TEST_CASE("visit_node")
{
    dryad::tree<node> tree;

    auto a = tree.create<leaf_node>();
    auto b = tree.create<leaf_node>();
    auto c = tree.create<leaf_node>();

    auto container = tree.create<container_node>();
    container->insert_front(a, b, c);

    SUBCASE("basic")
    {
        auto leaf_count      = 0;
        auto container_count = 0;

        auto visit = [&](auto node) {
            auto result = dryad::visit_node_all(
                node,
                [&](leaf_node*) {
                    ++leaf_count;
                    return 0;
                },
                [&](container_node*) {
                    ++container_count;
                    return 0;
                });
            CHECK(result == 0);
        };
        visit(a);
        visit(b);
        visit(c);
        visit(container);

        CHECK(leaf_count == 3);
        CHECK(container_count == 1);
    }
    SUBCASE("catch all")
    {
        auto leaf_count = 0;
        auto node_count = 0;

        auto visit = [&](auto n) {
            auto result = dryad::visit_node_all(
                n,
                [&](node*) {
                    ++node_count;
                    return 0;
                },
                [&](leaf_node*) {
                    ++leaf_count;
                    return 0;
                });
            CHECK(result == 0);
        };
        visit(a);
        visit(b);
        visit(c);
        visit(container);

        CHECK(leaf_count == 0);
        CHECK(node_count == 4);
    }
    SUBCASE("leaf only")
    {
        auto leaf_count = 0;

        auto visit = [&](auto node) { dryad::visit_node(node, [&](leaf_node*) { ++leaf_count; }); };
        visit(a);
        visit(b);
        visit(c);
        visit(container);

        CHECK(leaf_count == 3);
    }
}

