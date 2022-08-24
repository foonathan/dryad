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
        static_assert(is_node<T, NodeKind> && !is_abstract_node<T, NodeKind>);
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
        DRYAD_PRECONDITION(root != nullptr);
        DRYAD_PRECONDITION(!root->is_linked_in_tree());
        _root = root;
        _root->set_next_parent(_root);
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
                auto child = _cur->first_child();
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
            else if (_cur->next_node() == _cur)
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
    friend _traverse_range<node<NodeKind>> traverse(node<NodeKind>* n);
    template <typename NodeKind>
    friend _traverse_range<node<NodeKind>> traverse(const node<NodeKind>* n);
};

template <typename NodeKind>
_traverse_range<node<NodeKind>> traverse(node<NodeKind>* n)
{
    return _traverse_range<node<NodeKind>>(n);
}
template <typename NodeKind>
_traverse_range<node<NodeKind>> traverse(const node<NodeKind>* n)
{
    return _traverse_range<node<NodeKind>>(n);
}

template <typename NodeKind, typename MemoryResource>
auto traverse(tree<NodeKind, MemoryResource>& t)
{
    return traverse(t.root());
}
template <typename NodeKind, typename MemoryResource>
auto traverse(const tree<NodeKind, MemoryResource>& t)
{
    return traverse(t.root());
}
} // namespace dryad

namespace dryad::_detail
{
template <typename... T>
struct node_type_list
{
    template <typename U>
    using insert = node_type_list<U, T...>;
};

template <auto... LambdaCallOps>
struct _node_types_for_lambdas;
template <>
struct _node_types_for_lambdas<>
{
    using type = node_type_list<>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(T*), auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<std::decay_t<T>>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(T*) const, auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<std::decay_t<T>>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(traverse_event, T*), auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<std::decay_t<T>>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(traverse_event, T*) const, auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<std::decay_t<T>>;
};

template <typename... Lambdas>
using node_types_for_lambdas = typename _node_types_for_lambdas<&Lambdas::operator()...>::type;
} // namespace dryad::_detail

namespace dryad
{
template <typename NodeTypeList, typename... Lambdas>
struct _visit_tree;
template <typename... NodeTypes, typename... Lambdas>
struct _visit_tree<_detail::node_type_list<NodeTypes...>, Lambdas...>
{
    template <typename T, typename Node, typename Lambda>
    DRYAD_FORCE_INLINE static auto call(traverse_event ev, Node* node, Lambda&& lambda)
        -> decltype(lambda(ev, DRYAD_DECLVAL(T*)), true)
    {
        lambda(ev, static_cast<T*>(node));
        return true;
    }
    template <typename T, typename Node, typename Lambda>
    DRYAD_FORCE_INLINE static auto call(traverse_event ev, Node* node, Lambda& lambda)
        -> decltype(lambda(DRYAD_DECLVAL(T*)), true)
    {
        if (ev != traverse_event::exit)
            lambda(static_cast<T*>(node));
        return true;
    }

    template <bool All = false, typename TreeOrNode>
    DRYAD_FORCE_INLINE static void visit(TreeOrNode&& tree_or_node, Lambdas&&... lambdas)
    {
        for (auto [ev, node] : dryad::traverse(tree_or_node))
        {
            auto                  kind = node->kind();
            [[maybe_unused]] auto found_callback
                = ((NodeTypes::type_matches_kind(kind)
                        ? call<NodeTypes>(ev, node_cast<NodeTypes>(node), lambdas)
                        : false)
                   || ...);

            if constexpr (All)
                DRYAD_ASSERT(found_callback, "missing type for callback");
        }
    }
};

/// Traverses the tree invoking the appropriate lambda for each node type.
///
/// * If a lambda has signature (traverse_event, NodeType), will invoke it for every NodeType.
/// * If a lambda has signature (NodeType), will invoke it on enter/leaf events only.
///
/// It will try each lambda in the order specified, NodeType can be abstract in which case it
/// swallows all. Only one lambda will be invoked. If the type of a node does not match any lambda,
/// it will not be invoked.
template <typename NodeKind, typename... Lambdas>
void visit(node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<node_types, Lambdas...>::visit(node, DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename... Lambdas>
void visit(const node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<node_types, Lambdas...>::visit(node, DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit(tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit(tree.root(), DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit(const tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit(tree.root(), DRYAD_FWD(lambdas)...);
}

/// Same as above, but it is an error if a node cannot be visited.
template <typename NodeKind, typename... Lambdas>
void visit_all(node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<node_types, Lambdas...>::template visit<true>(node, DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename... Lambdas>
void visit_all(const node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<node_types, Lambdas...>::template visit<true>(node, DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit_all(tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit_all(tree.root(), DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit_all(const tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit_all(tree.root(), DRYAD_FWD(lambdas)...);
}
} // namespace dryad

#endif // DRYAD_TREE_HPP_INCLUDED

