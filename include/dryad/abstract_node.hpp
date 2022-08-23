// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_ABSTRACT_NODE_HPP_INCLUDED
#define DRYAD_ABSTRACT_NODE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/node.hpp>

namespace dryad
{
/// Base class for the abstract base class of the specified nodes.
///
/// It can be used to inject common data into the specified node kinds.
/// Injection only happens when it is specified as the AbstractBase in the node definition.
template <typename AbstractBase, typename AbstractBase::node_kind_type... Kinds>
class abstract_node : public AbstractBase
{
public:
    static constexpr bool type_is_abstract()
    {
        return true;
    }
    static constexpr bool type_matches_kind(typename AbstractBase::node_kind_type kind)
    {
        using traits = node_kind_traits<typename AbstractBase::node_kind_type>;
        return ((traits::to_int(Kinds) == traits::to_int(kind)) || ...);
    }

protected:
    using node_base = abstract_node;
    explicit abstract_node(node_ctor ctor, typename AbstractBase::node_kind_type kind)
    : AbstractBase(ctor, kind)
    {}
    ~abstract_node() = default;
};

/// Base class for the abstract base class of the specified nodes.
///
/// It can be used to inject common data into the specified node kinds.
/// Injection only happens when it is specified as the AbstractBase in the node definition.
template <typename AbstractBase, typename AbstractBase::node_kind_type FirstKind,
          typename AbstractBase::node_kind_type LastKind>
class abstract_node_range : public AbstractBase
{
public:
    static constexpr bool type_is_abstract()
    {
        return true;
    }
    static constexpr bool type_matches_kind(typename AbstractBase::node_kind_type kind)
    {
        using traits = node_kind_traits<typename AbstractBase::node_kind_type>;
        return traits::to_int(FirstKind) <= traits::to_int(kind)
               && traits::to_int(kind) <= traits::to_int(LastKind);
    }

protected:
    using node_base = abstract_node_range;
    explicit abstract_node_range(node_ctor ctor, typename AbstractBase::node_kind_type kind)
    : AbstractBase(ctor, kind)
    {}
    ~abstract_node_range() = default;
};

/// Base class for the abstract base class of all nodes.
///
/// It can be used to inject common data into all nodes.
/// Injection only happens when it is specified as the AbstractBase in the node definition.
template <typename NodeKind>
class abstract_node_all : public node<NodeKind>
{
protected:
    using node_base = abstract_node_all;
    explicit abstract_node_all(node_ctor ctor, NodeKind kind) : node<NodeKind>(ctor, kind) {}
    ~abstract_node_all() = default;
};

#define DRYAD_ABSTRACT_NODE_CTOR(Name)                                                             \
    explicit Name(::dryad::node_ctor ctor, typename Name::node_kind_type kind)                     \
    : node_base(ctor, kind)                                                                        \
    {}
} // namespace dryad

#endif // DRYAD_ABSTRACT_NODE_HPP_INCLUDED

