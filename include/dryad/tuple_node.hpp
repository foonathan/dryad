// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_TUPLE_NODE_HPP_INCLUDED
#define DRYAD_TUPLE_NODE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/_detail/tuple.hpp>
#include <dryad/node.hpp>
#include <initializer_list>

namespace dryad
{
/// Base class for nodes that contain a single child.
///
/// The child nodes must be given in the constructor and cannot be replaced.
template <typename AbstractBase, auto NodeKind,
          typename ChildT = node<DRYAD_DECAY_DECLTYPE(NodeKind)>>
class single_node : public basic_container_node<AbstractBase, NodeKind>
{
public:
    ChildT* child()
    {
        return _child;
    }
    const ChildT* child() const
    {
        return _child;
    }

protected:
    using node_base = single_node;

    explicit single_node(node_ctor ctor, ChildT* child)
    : basic_container_node<AbstractBase, NodeKind>(ctor), _child(child)
    {
        this->insert_first_child(child);
    }

    ~single_node() = default;

private:
    ChildT* _child;
};
} // namespace dryad

namespace dryad
{
/// Base class for nodes that contain N nodes of the specified type.
///
/// The child nodes must be given in the constructor and cannot be replaced.
template <typename AbstractBase, auto NodeKind, std::size_t N,
          typename ChildT = node<DRYAD_DECAY_DECLTYPE(NodeKind)>>
class array_node : public basic_container_node<AbstractBase, NodeKind>
{
    static_assert(N >= 1);

public:
    template <typename T>
    struct _children_range
    {
        using iterator = T**;

        static constexpr bool empty()
        {
            return false;
        }
        static constexpr std::size_t size()
        {
            return N;
        }

        T* operator[](std::size_t idx) const
        {
            DRYAD_PRECONDITION(idx < size());
            return _self->_children[idx];
        }

        iterator begin() const
        {
            // If T == ChildT, cast is noop.
            // If T == const ChildT, we cast a `ChildT**` to `const ChildT**`, which is allowed.
            return (iterator)_self->_children;
        }
        iterator end() const
        {
            return (iterator)_self->_children + N;
        }

        const array_node* _self;
    };

    _children_range<ChildT> children()
    {
        return {this};
    }
    _children_range<const ChildT> children() const
    {
        return {this};
    }

protected:
    using node_base = array_node;

    explicit array_node(node_ctor ctor, std::initializer_list<ChildT*> children)
    : basic_container_node<AbstractBase, NodeKind>(ctor)
    {
        DRYAD_PRECONDITION(children.size() == N);
        this->insert_first_child(*children.begin());
        for (auto iter = children.begin(); iter != children.end() - 1; ++iter)
            this->insert_child_after(iter[0], iter[1]);
    }

    ~array_node() = default;

private:
    ChildT* _children[N];
};
} // namespace dryad

namespace dryad
{
/// Base class for nodes that contain the specified child types.
///
/// The child nodes must be given in the constructor and cannot be replaced.
template <typename AbstractBase, auto NodeKind, typename... ChildTs>
class tuple_node : public basic_container_node<AbstractBase, NodeKind>
{
    static_assert(sizeof...(ChildTs) > 0);

public:
    template <std::size_t N>
    auto child()
    {
        return _children.template get<N>();
    }
    template <std::size_t N>
    auto child() const
    {
        return DRAYD_AS_CONST(_children.template get<N>());
    }

protected:
    using node_base = tuple_node;

    template <typename H, typename... T>
    explicit tuple_node(node_ctor ctor, H* head, T*... tail)
    : basic_container_node<AbstractBase, NodeKind>(ctor), _children(head, tail...)
    {
        this->insert_first_child(head);

        auto pos = static_cast<node<DRYAD_DECAY_DECLTYPE(NodeKind)>*>(head);
        ((this->insert_child_after(pos, tail), pos = tail), ...);
    }

    ~tuple_node() = default;

private:
    _detail::tuple<ChildTs*...> _children;
};

template <typename AbstractBase, auto NodeKind, typename FirstChild,
          typename SecondChild = FirstChild>
class pair_node : public tuple_node<AbstractBase, NodeKind, FirstChild, SecondChild>
{
public:
    FirstChild* first_child()
    {
        return this->template child<0>();
    }
    const FirstChild* first_child() const
    {
        return this->template child<0>();
    }

    SecondChild* second_child()
    {
        return this->template child<1>();
    }
    const SecondChild* second_child() const
    {
        return this->template child<1>();
    }

protected:
    using node_base = pair_node;

    explicit pair_node(node_ctor ctor, FirstChild* first, SecondChild* second)
    : tuple_node<AbstractBase, NodeKind, FirstChild, SecondChild>(ctor, first, second)
    {}

    ~pair_node() = default;
};

template <typename AbstractBase, auto NodeKind, typename LeftChild, typename RightChild = LeftChild>
class binary_node : public pair_node<AbstractBase, NodeKind, LeftChild, RightChild>
{
public:
    LeftChild* left_child()
    {
        return this->template child<0>();
    }
    const LeftChild* left_child() const
    {
        return this->template child<0>();
    }

    RightChild* right_child()
    {
        return this->template child<1>();
    }
    const RightChild* right_child() const
    {
        return this->template child<1>();
    }

protected:
    using node_base = binary_node;

    explicit binary_node(node_ctor ctor, LeftChild* first, RightChild* second)
    : pair_node<AbstractBase, NodeKind, LeftChild, RightChild>(ctor, first, second)
    {}

    ~binary_node() = default;
};
} // namespace dryad

#endif // DRYAD_TUPLE_NODE_HPP_INCLUDED

