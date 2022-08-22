// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_TREE_HPP_INCLUDED
#define DRYAD_TREE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/arena.hpp>
#include <dryad/node.hpp>

namespace dryad
{
template <typename NodeKind, typename MemoryResource = void>
class tree
{
public:
    //=== construction ===//
    tree() = default;
    explicit tree(MemoryResource* resource) : _arena(resource) {}

    tree(tree&&) noexcept            = default;
    tree& operator=(tree&&) noexcept = default;

    ~tree() = default;

    //=== node creation ===//
    /// Creates a node of type T that is not yet linked into the tree.
    template <typename T, typename... Args>
    T* create(Args&&... args)
    {
        static_assert(is_defined_node<T, NodeKind>);
        static_assert(std::is_trivially_destructible_v<T>, "nobody will call its destructor");

        return _arena.template construct<T>(node_ctor{}, DRYAD_FWD(args)...);
    }

    //=== root node ===//
    node<NodeKind>* root()
    {
        return _root;
    }
    const node<NodeKind>* root() const
    {
        return _root;
    }

    void set_root(node<NodeKind>* root)
    {
        _root = root;
    }

private:
    arena<MemoryResource> _arena;
    node<NodeKind>*       _root = nullptr;
};
} // namespace dryad

namespace dryad
{
enum class traverse_event
{
    /// We're visiting a container node before all its children.
    enter,
    /// We're visiting a container node after all its children.
    exit,
    /// We're visiting a non-container.
    leaf,
};

template <typename T>
class _traverse_range
{
public:
    struct _value_type
    {
        traverse_event event;
        T*             node;
    };

    class iterator : public _detail::forward_iterator_base<iterator, _value_type, _value_type, void>
    {
    public:
        iterator() = default;

        _value_type deref() const
        {
            return {_ev, _cur};
        }

        void increment()
        {
            if (_ev == traverse_event::enter)
            {
                auto container = static_cast<container_node<typename T::node_kind_type>*>(_cur);
                auto child     = container->first_child();
                if (child != nullptr)
                {
                    // We go to the first child next.
                    if (child->is_container())
                        _ev = traverse_event::enter;
                    else
                        _ev = traverse_event::leaf;

                    _cur = child;
                }
                else
                {
                    // Don't have children, exit.
                    _ev = traverse_event::exit;
                }
            }
            else if (_cur->next_node() == nullptr)
            {
                // We're done.
                _cur = nullptr;
                _ev  = traverse_event::leaf;
            }
            else
            {
                // We follow the next pointer.

                if (_cur->next_node_is_parent())
                    // We go back to a production for the second time.
                    _ev = traverse_event::exit;
                else if (_cur->next_node()->is_container())
                    // We're having a production as sibling.
                    _ev = traverse_event::enter;
                else
                    // Token as sibling.
                    _ev = traverse_event::leaf;

                _cur = _cur->next_node();
            }
        }

        bool equal(iterator rhs) const
        {
            return _ev == rhs._ev && _cur == rhs._cur;
        }

    private:
        T*             _cur = nullptr;
        traverse_event _ev  = traverse_event::leaf;

        friend _traverse_range;
    };

    bool empty() const
    {
        return _begin == _end;
    }

    iterator begin() const
    {
        return _begin;
    }

    iterator end() const
    {
        return _end;
    }

private:
    _traverse_range(T* node)
    {
        if (node == nullptr)
            return;

        if (node->is_container())
        {
            _begin._cur = node;
            _begin._ev  = traverse_event::enter;

            _end._cur = node;
            _end._ev  = traverse_event::exit;
            ++_end; // half-open range
        }
        else
        {
            _begin._cur = node;
            _begin._ev  = traverse_event::leaf;

            _end = _begin;
            ++_end;
        }
    }

    iterator _begin, _end;

    template <typename NodeKind>
    friend auto traverse(node<NodeKind>* n);
    template <typename NodeKind>
    friend auto traverse(const node<NodeKind>* n);
};

template <typename NodeKind>
auto traverse(node<NodeKind>* n)
{
    return _traverse_range<node<NodeKind>>(n);
}
template <typename NodeKind>
auto traverse(const node<NodeKind>* n)
{
    return _traverse_range<node<NodeKind>>(n);
}

template <typename NodeKind>
auto traverse(tree<NodeKind>& t)
{
    return traverse(t.root());
}
template <typename NodeKind>
auto traverse(const tree<NodeKind>& t)
{
    return traverse(t.root());
}
} // namespace dryad

#endif // DRYAD_TREE_HPP_INCLUDED

