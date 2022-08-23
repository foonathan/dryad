// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_OPTIONAL_NODE_HPP_INCLUDED
#define DRYAD_OPTIONAL_NODE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/node.hpp>

namespace dryad
{
/// Base class for nodes that contain no child or a a single child.
template <typename AbstractBase, auto NodeKind,
          typename ChildT = node<DRYAD_DECAY_DECLTYPE(NodeKind)>>
class optional_node : public basic_container_node<AbstractBase, NodeKind>
{
public:
    //=== access ===//
    bool has_child() const
    {
        return _child != nullptr;
    }

    ChildT* child()
    {
        return _child;
    }
    const ChildT* child() const
    {
        return _child;
    }

    //=== modifiers ===//
    void insert_child(ChildT* child)
    {
        this->insert_first_child(child);
        _child = child;
    }

    ChildT* erase_child()
    {
        return static_cast<ChildT*>(this->erase_child_after(nullptr));
    }

protected:
    using node_base = optional_node;

    explicit optional_node(node_ctor ctor) : basic_container_node<AbstractBase, NodeKind>(ctor) {}

    ~optional_node() = default;

private:
    ChildT* _child = nullptr;
};
} // namespace dryad

#endif // DRYAD_OPTIONAL_NODE_HPP_INCLUDED
