// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_LIST_NODE_HPP_INCLUDED
#define DRYAD_LIST_NODE_HPP_INCLUDED

#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/iterator.hpp>
#include <dryad/node.hpp>

namespace dryad
{
template <typename T>
struct _list_node_children_range
{
    struct iterator : _detail::forward_iterator_base<iterator, T*, T*, void>
    {
        const node<typename T::node_kind_type>* _cur = nullptr;

        operator typename _list_node_children_range<const T>::iterator() const
        {
            return {_cur};
        }

        operator T*() const
        {
            return deref();
        }
        T* deref() const
        {
            return (T*)_cur;
        }
        void increment()
        {
            _cur = _cur->next_node();
        }
        bool equal(iterator rhs) const
        {
            return _cur == rhs._cur;
        }
    };

    bool empty() const
    {
        return _self->first_child() == nullptr;
    }

    std::size_t size() const
    {
        return _self->user_data32();
    }

    iterator begin() const
    {
        return {{}, _self->first_child()};
    }
    iterator end() const
    {
        if (!empty())
            // The last child has a pointer back to self.
            return {{}, _self};
        else
            // begin() == nullptr, so return that as well.
            return {};
    }

    const container_node<typename T::node_kind_type>* _self;
};

/// Base class for nodes that contain a linked-list of child nodes.
template <auto NodeKind, typename ChildT = node<DRYAD_DECAY_DECLTYPE(NodeKind)>>
class list_node : public basic_container_node<NodeKind>
{
public:
    using node_kind_type = DRYAD_DECAY_DECLTYPE(NodeKind);

    //=== access ===//
    _list_node_children_range<ChildT> children()
    {
        return {this};
    }
    _list_node_children_range<const ChildT> children() const
    {
        return {this};
    }

    //=== modifiers ===//
    using iterator = typename _list_node_children_range<ChildT>::iterator;

    iterator insert_front(ChildT* child)
    {
        DRYAD_PRECONDITION(!child->is_linked_in_tree());

        if (this->first_child() == nullptr)
            this->insert_first_child(child);
        else
            this->insert_child_front(child);
        ++this->user_data32();

        return {{}, child};
    }

    iterator insert_after(iterator pos, ChildT* child)
    {
        DRYAD_PRECONDITION(!child->is_linked_in_tree());

        if (pos._cur == nullptr)
            this->insert_first_child(child);
        else
            this->insert_child_after(pos, child);
        ++this->_user_data32();

        return {{}, child};
    }

    ChildT* erase_after(iterator pos)
    {
        return static_cast<ChildT*>(this->erase_child_after(pos));
    }

protected:
    using node_base = list_node;
    explicit list_node(node_ctor ctor) : basic_container_node<NodeKind>(ctor)
    {
        static_assert(dryad::is_node<ChildT, node_kind_type>);
    }
    ~list_node() = default;

private:
    // We use user_data32 to store the number of children.
    using basic_container_node<NodeKind>::user_data32;

    template <typename T>
    friend struct _list_node_children_range;
};
} // namespace dryad

#endif // DRYAD_NODE_LIST_HPP_INCLUDED

