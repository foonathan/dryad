// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_OPTIONAL_NODE_HPP_INCLUDED
#define DRYAD_OPTIONAL_NODE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/node.hpp>

namespace dryad
{
/// Abstract base class for nodes that contain no child or a a single child.
template <typename AbstractBase, typename ChildT = node<typename AbstractBase::node_kind_type>>
class optional_node : public container_node<AbstractBase>
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
        DRYAD_PRECONDITION(child != nullptr && !child->is_linked_in_tree());
        this->insert_child_after(nullptr, child);
    }

    ChildT* erase_child()
    {
        return static_cast<ChildT*>(this->erase_child_after(nullptr));
    }

    ChildT* replace_child(ChildT* new_child)
    {
        DRYAD_PRECONDITION(new_child != nullptr && !new_child->is_linked_in_tree());
        if (has_child())
        {
            auto old = this->erase_child_after(nullptr);
            this->insert_child_after(nullptr, new_child);
            return static_cast<ChildT*>(old);
        }
        else
        {
            insert_child(new_child);
            return nullptr;
        }
    }

protected:
    using node_base = optional_node;

    explicit optional_node(node_ctor ctor, typename AbstractBase::node_kind_type kind)
    : container_node<AbstractBase>(ctor, kind)
    {}

    ~optional_node() = default;
};
} // namespace dryad

#endif // DRYAD_OPTIONAL_NODE_HPP_INCLUDED

