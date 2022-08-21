// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_TREE_HPP_INCLUDED
#define DRYAD_TREE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/node.hpp>

namespace dryad
{
template <typename NodeKind>
class tree
{
public:
    //=== construction ===//
    tree() = default;

    tree(tree&&) noexcept            = default;
    tree& operator=(tree&&) noexcept = default;

    ~tree() = default;

    //=== node creation ===//
    template <typename T, typename... Args>
    T* create(Args&&... args)
    {
        static_assert(is_defined_node<T, NodeKind>);

        // TODO: memory management
        return new T(node_ctor{}, DRYAD_FWD(args)...);
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
    node<NodeKind>* _root = nullptr;
};
} // namespace dryad

#endif // DRYAD_TREE_HPP_INCLUDED

