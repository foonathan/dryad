// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_NODE_LIST_HPP_INCLUDED
#define DRYAD_NODE_LIST_HPP_INCLUDED

#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/iterator.hpp>
#include <dryad/node.hpp>

namespace dryad
{
/// Base class for nodes that contain a linked-list of child nodes.
template <typename NodeKind, typename ChildT = node<NodeKind>>
class node_list : public node_container<NodeKind>
{
public:
    //=== access ===//
    template <typename T>
    struct _children_range
    {
        struct iterator : _detail::forward_iterator_base<iterator, T*, T*, void>
        {
            const node<NodeKind>* _cur = nullptr;

            operator typename _children_range<const T>::iterator() const
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

        const node_list* _self;
    };

    _children_range<ChildT> children()
    {
        return {this};
    }
    _children_range<const ChildT> children() const
    {
        return {this};
    }

    //=== modifiers ===//
    using iterator = typename _children_range<node<NodeKind>>::iterator;

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
    explicit node_list(node_ctor ctor, NodeKind kind) : node_container<NodeKind>(ctor, kind)
    {
        static_assert(dryad::is_node<ChildT, NodeKind>);
    }

    ~node_list() = default;

private:
    // We use user_data32 to store the number of children.
    using node_container<NodeKind>::user_data32;
};
} // namespace dryad

#endif // DRYAD_NODE_LIST_HPP_INCLUDED

