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
/// Abstract base class for nodes that contain a single child.
template <typename AbstractBase, typename ChildT = node<typename AbstractBase::node_kind_type>>
class single_node : public container_node<AbstractBase>
{
public:
    //=== access ===//
    ChildT* child()
    {
        return static_cast<ChildT*>(*this->children().begin());
    }
    const ChildT* child() const
    {
        return static_cast<const ChildT*>(*this->children().begin());
    }

    //=== modifier ===//
    ChildT* replace_child(ChildT* new_child)
    {
        DRYAD_PRECONDITION(new_child != nullptr && !new_child->is_linked_in_tree());
        auto old = this->erase_child_after(nullptr);
        this->insert_child_after(nullptr, new_child);
        return static_cast<ChildT*>(old);
    }

protected:
    using node_base = single_node;

    explicit single_node(node_ctor ctor, typename AbstractBase::node_kind_type kind, ChildT* child)
    : container_node<AbstractBase>(ctor, kind)
    {
        this->insert_child_after(nullptr, child);
    }

    ~single_node() = default;
};
} // namespace dryad

namespace dryad
{
/// Abstract base class for nodes that contain N nodes of the specified type.
template <typename AbstractBase, std::size_t N,
          typename ChildT = node<typename AbstractBase::node_kind_type>>
class array_node : public container_node<AbstractBase>
{
    static_assert(N >= 1);

public:
    //=== access ===//
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

    //=== modifier ===//
    ChildT* replace_child(std::size_t idx, ChildT* new_child)
    {
        DRYAD_PRECONDITION(new_child != nullptr && !new_child->is_linked_in_tree());
        _children[idx] = new_child;

        auto pos = idx == 0 ? nullptr : _children[idx - 1];
        auto old = this->erase_child_after(pos);
        this->insert_child_after(pos, new_child);
        return static_cast<ChildT*>(old);
    }

protected:
    using node_base = array_node;

    explicit array_node(node_ctor ctor, typename AbstractBase::node_kind_type kind,
                        std::initializer_list<ChildT*> children)
    : container_node<AbstractBase>(ctor, kind)
    {
        auto idx = 0;
        for (auto child : children)
        {
            DRYAD_PRECONDITION(child);
            _children[idx++] = child;
        }

        DRYAD_PRECONDITION(children.size() == N);
        this->insert_child_after(nullptr, *children.begin());
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
/// Abstract base class for nodes that contain the specified child types.
template <typename AbstractBase, typename HeadChildT, typename... TailChildTs>
class tuple_node : public container_node<AbstractBase>
{
public:
    //=== access ===//
    template <std::size_t N>
    auto child()
    {
        if constexpr (N == 0)
            return static_cast<HeadChildT*>(*this->children().begin());
        else
            return _children.template get<N - 1>();
    }
    template <std::size_t N>
    auto child() const
    {
        if constexpr (N == 0)
            return static_cast<const HeadChildT*>(this->children().begin());
        else
            return DRAYD_AS_CONST(_children.template get<N - 1>());
    }

    //=== modifiers ===//
    template <std::size_t N, typename ChildT>
    ChildT* replace_child(ChildT* new_child)
    {
        DRYAD_PRECONDITION(new_child != nullptr && !new_child->is_linked_in_tree());
        if constexpr (N == 0)
        {
            auto old = this->erase_child_after(nullptr);
            this->insert_child_after(nullptr, new_child);
            return static_cast<ChildT*>(old);
        }
        else
        {
            _children.template get<N - 1>() = new_child;

            auto pos = child<N - 1>();
            auto old = this->erase_child_after(pos);
            this->insert_child_after(pos, new_child);
            return static_cast<ChildT*>(old);
        }
    }

protected:
    using node_base = tuple_node;

    template <typename H, typename... T>
    explicit tuple_node(node_ctor ctor, typename AbstractBase::node_kind_type kind, H* head,
                        T*... tail)
    : container_node<AbstractBase>(ctor, kind), _children(tail...)
    {
        DRYAD_PRECONDITION(((head != nullptr) && ... && (tail != nullptr)));
        this->insert_child_after(nullptr, head);

        auto pos = *this->children().begin();
        ((this->insert_child_after(pos, tail), pos = tail), ...);
    }

    ~tuple_node() = default;

private:
    _detail::tuple<TailChildTs*...> _children;
};

template <typename AbstractBase, typename LeftChild, typename RightChild = LeftChild>
class binary_node : public tuple_node<AbstractBase, LeftChild, RightChild>
{
public:
    //=== access ===//
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

    //=== modifiers ===//
    LeftChild* replace_left_child(LeftChild* new_child)
    {
        return this->template replace_child<0>(new_child);
    }
    RightChild* replace_right_child(RightChild* new_child)
    {
        return this->template replace_child<1>(new_child);
    }

protected:
    using node_base = binary_node;

    explicit binary_node(node_ctor ctor, typename AbstractBase::node_kind_type kind,
                         LeftChild* first, RightChild* second)
    : tuple_node<AbstractBase, LeftChild, RightChild>(ctor, kind, first, second)
    {}

    ~binary_node() = default;
};
} // namespace dryad

#endif // DRYAD_TUPLE_NODE_HPP_INCLUDED

