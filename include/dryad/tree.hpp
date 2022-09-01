// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_TREE_HPP_INCLUDED
#define DRYAD_TREE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/arena.hpp>
#include <dryad/node.hpp>

namespace dryad
{
/// Owns multiple nodes.
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

using traverse_event_enter = std::integral_constant<traverse_event, traverse_event::enter>;
using traverse_event_exit  = std::integral_constant<traverse_event, traverse_event::exit>;

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
                if (_cur->has_children())
                {
                    // We go to the first child next.
                    auto first_child = _cur->children().front();
                    if (first_child->has_children())
                        _ev = traverse_event::enter;
                    else
                        _ev = traverse_event::leaf;

                    _cur = first_child;
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
                    // We go back to a container for the second time.
                    _ev = traverse_event::exit;
                else if (_cur->next_node()->has_children())
                    // We're having a container as sibling.
                    _ev = traverse_event::enter;
                else
                    // Non-container as sibling.
                    _ev = traverse_event::leaf;

                _cur = _cur->next_node();
            }
        }

        void skip_children()
        {
            DRYAD_PRECONDITION(_ev == traverse_event::enter);
            _ev = traverse_event::exit;
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

        if (node->has_children())
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
    friend _traverse_range<const node<NodeKind>> traverse(const node<NodeKind>* n);
};

template <typename NodeKind>
_traverse_range<node<NodeKind>> traverse(node<NodeKind>* n)
{
    return _traverse_range<node<NodeKind>>(n);
}
template <typename NodeKind>
_traverse_range<const node<NodeKind>> traverse(const node<NodeKind>* n)
{
    return _traverse_range<const node<NodeKind>>(n);
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

namespace dryad
{
/// Visitor that can be used to visit the children of a node manually.
template <typename NodeKind>
struct child_visitor
{
    void* _ptr;
    void (*_fn)(void*, const node<NodeKind>*);

    void operator()(const node<NodeKind>* child) const
    {
        _fn(_ptr, child);
    }
};

/// Use as lambda to indicate that you don't want to visit a node or its children.
template <typename NodeType>
constexpr auto ignore_node
    = [](child_visitor<typename NodeType::node_kind_type>, const NodeType*) {};

template <bool All, typename NodeKind, typename NodeTypeList, typename... Lambdas>
struct _visit_tree;
template <bool All, typename NodeKind, typename... NodeTypes, typename... Lambdas>
struct _visit_tree<All, NodeKind, _detail::node_type_list<NodeTypes...>, Lambdas...>
: std::decay_t<Lambdas>...
{
    using node_kind                = std::decay_t<NodeKind>;
    static constexpr auto is_const = std::is_const_v<NodeKind>;

    DRYAD_FORCE_INLINE _visit_tree(Lambdas&&... lambdas)
    : std::decay_t<Lambdas>(DRYAD_FWD(lambdas))...
    {}

    template <typename T, typename Iter, typename Lambda>
    DRYAD_FORCE_INLINE auto call(_detail::priority_tag<4>, Iter& iter, Lambda& lambda)
        -> decltype(lambda(child_visitor<node_kind>{}, DRYAD_DECLVAL(T*)), true)
    {
        auto node = node_try_cast<T>(iter->node);
        if (node == nullptr)
            return false;

        if (iter->event == traverse_event::enter)
        {
            auto child_visit = [](void* _self, const dryad::node<node_kind>* child) {
                auto& self = *static_cast<_visit_tree*>(_self);

                // We might need to cast away the const that was added to maintain the function
                // pointer type.
                if constexpr (is_const)
                    self.visit(child);
                else
                    self.visit(const_cast<dryad::node<node_kind>*>(child));
            };
            lambda(child_visitor<node_kind>{this, child_visit}, node);
            iter.skip_children();
        }
        return true;
    }
    template <typename T, typename Iter, typename Lambda>
    DRYAD_FORCE_INLINE static auto call(_detail::priority_tag<3>, Iter& iter, Lambda& lambda)
        -> decltype(lambda(traverse_event::enter, DRYAD_DECLVAL(T*)), true)
    {
        auto node = node_try_cast<T>(iter->node);
        if (node == nullptr)
            return false;

        lambda(iter->event, node);
        return true;
    }
    template <typename T, typename Iter, typename Lambda>
    DRYAD_FORCE_INLINE static auto call(_detail::priority_tag<2>, Iter& iter, Lambda& lambda)
        -> decltype(lambda(traverse_event_enter{}, DRYAD_DECLVAL(T*)), true)
    {
        auto node = node_try_cast<T>(iter->node);
        if (node == nullptr)
            return false;

        if (iter->event == traverse_event::enter)
            lambda(traverse_event_enter{}, node);
        return true;
    }
    template <typename T, typename Iter, typename Lambda>
    DRYAD_FORCE_INLINE static auto call(_detail::priority_tag<2>, Iter& iter, Lambda& lambda)
        -> decltype(lambda(traverse_event_exit{}, DRYAD_DECLVAL(T*)), true)
    {
        auto node = node_try_cast<T>(iter->node);
        if (node == nullptr)
            return false;

        if (iter->event == traverse_event::exit)
            lambda(traverse_event_exit{}, node);
        return true;
    }
    template <typename T, typename Iter, typename Lambda>
    DRYAD_FORCE_INLINE static auto call(_detail::priority_tag<2>, Iter& iter, Lambda& lambda)
        -> decltype(lambda(DRYAD_DECLVAL(T*)), true)
    {
        auto node = node_try_cast<T>(iter->node);
        if (node == nullptr)
            return false;

        if (iter->event != traverse_event::exit)
            lambda(node);
        return true;
    }

    template <typename TreeOrNode>
    DRYAD_FORCE_INLINE void visit(TreeOrNode&& tree_or_node)
    {
        auto range = dryad::traverse(tree_or_node);
        for (auto iter = range.begin(); iter != range.end(); ++iter)
        {
            [[maybe_unused]] auto found_callback
                = (call<NodeTypes>(_detail::priority_tag<4>{}, iter,
                                   static_cast<const Lambdas&>(*this))
                   || ...);
            if constexpr (All)
                DRYAD_ASSERT(found_callback, "missing type for callback");
        }
    }
};

/// Traverses the (sub)tree invoking the appropriate lambda for each node type.
///
/// * If a lambda has signature (traverse_event, NodeType), will invoke it every time NodeType is
/// visited.
/// * If a lambda has signature (traverse_event_enter, NodeType), will invoke it on enter events
/// only.
/// * If a lambda has signature (traverse_event_exit, NodeType), will invoke it on exit events only.
/// * If a lambda has signature (NodeType), will invoke it on enter or leaf events only.
/// * If a lambda has signature (child_visitor, NodeType), will invoke it on enter events only and
///   not for any child nodes. Children have to be visited manually by passing them to the
///   child_visitor.
///
/// It will try each lambda in the order specified, NodeType can be abstract in which case it
/// swallows all. Only one lambda will be invoked. If the type of a node does not match any lambda,
/// it will not be invoked.
template <typename NodeKind, typename... Lambdas>
void visit_tree(node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<false, NodeKind, node_types, Lambdas...>(DRYAD_FWD(lambdas)...).visit(node);
}
template <typename NodeKind, typename... Lambdas>
void visit_tree(const node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<false, const NodeKind, node_types, Lambdas...>(DRYAD_FWD(lambdas)...).visit(node);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit_tree(tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit_tree(tree.root(), DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit_tree(const tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit_tree(tree.root(), DRYAD_FWD(lambdas)...);
}

/// Same as above, but it is an error if a node cannot be visited.
template <typename NodeKind, typename... Lambdas>
void visit_tree_all(node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<true, NodeKind, node_types, Lambdas...>(DRYAD_FWD(lambdas)...).visit(node);
}
template <typename NodeKind, typename... Lambdas>
void visit_tree_all(const node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_tree<true, const NodeKind, node_types, Lambdas...>(DRYAD_FWD(lambdas)...).visit(node);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit_tree_all(tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit_tree_all(tree.root(), DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename MemoryResource, typename... Lambdas>
void visit_tree_all(const tree<NodeKind, MemoryResource>& tree, Lambdas&&... lambdas)
{
    visit_tree_all(tree.root(), DRYAD_FWD(lambdas)...);
}
} // namespace dryad

#endif // DRYAD_TREE_HPP_INCLUDED

