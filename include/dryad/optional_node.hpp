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
        return this->first_child() != nullptr;
    }

    ChildT* child()
    {
        return static_cast<ChildT*>(this->first_child());
    }
    const ChildT* child() const
    {
        return static_cast<const ChildT*>(this->first_child());
    }

    //=== modifiers ===//
    void insert_child(ChildT* child)
    {
        this->insert_first_child(child);
    }

    ChildT* erase_child()
    {
        return static_cast<ChildT*>(this->erase_child_after(nullptr));
    }

    ChildT* replace_child(ChildT* new_child)
    {
        auto old = this->erase_child_after(nullptr);
        this->insert_first_child(new_child);
        return static_cast<ChildT*>(old);
    }

protected:
    using node_base = optional_node;

    explicit optional_node(node_ctor ctor) : basic_container_node<AbstractBase, NodeKind>(ctor) {}

    ~optional_node() = default;
};
} // namespace dryad

#endif // DRYAD_OPTIONAL_NODE_HPP_INCLUDED

