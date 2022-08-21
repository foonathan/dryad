// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_NODE_HPP_INCLUDED
#define DRYAD_NODE_HPP_INCLUDED

#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>

namespace dryad
{
template <typename NodeKind>
class node;
template <typename NodeKind>
class node_container;
} // namespace dryad

namespace dryad
{
/// It is sometimes useful to give each node a color during certain tree algorithms, e.g. cycle
/// detection. This enum represents a generic color that can be set/queried for every node as
/// necessary.
///
/// The meaning of the individual colors depends on the algorithm in question.
enum class color
{
    uncolored,
    black,
    grey,
    white,
};
} // namespace dryad

namespace dryad
{
template <typename NodeKind>
struct node_kind_traits
{
    static constexpr std::uint16_t to_int(NodeKind kind)
    {
        if constexpr (std::is_enum_v<NodeKind>)
        {
            auto underlying = static_cast<std::underlying_type_t<NodeKind>>(kind);
            DRYAD_PRECONDITION(0 <= underlying && underlying <= 0x7FFF);
            return static_cast<std::uint16_t>(underlying);
        }
        else
        {
            DRYAD_PRECONDITION(0 <= kind && kind <= 0x7FFF);
            return static_cast<std::uint16_t>(kind);
        }
    }

    static constexpr NodeKind from_int(std::uint16_t v)
    {
        return static_cast<NodeKind>(v);
    }
};

template <typename T, typename NodeKind>
constexpr bool is_node = std::is_base_of_v<node<NodeKind>, T>;
} // namespace dryad

namespace dryad
{
class node_ctor
{
    node_ctor() = default;

    template <typename NodeKind>
    friend class tree;
};

/// Base class for all nodes in the AST.
template <typename NodeKind>
class node
{
    using _traits = node_kind_traits<NodeKind>;

public:
    NodeKind kind() const
    {
        return _traits::from_int(_kind);
    }

    //=== tree relationship ===//
    node* next_node() const
    {
        return reinterpret_cast<node*>(_ptr & ~0b111);
    }

    bool is_linked_in_tree() const
    {
        return next_node() != nullptr;
    }

    /// Root node returns a pointer to itself.
    const node* parent() const
    {
        if (!is_linked_in_tree())
            return nullptr;

        // If we follow the sibling pointer repeatedly, we end up at the parent eventually.
        auto cur = this;
        while (!cur->next_is_parent())
            cur = cur->next_node();
        return cur->next_node();
    }
    node* parent()
    {
        return const_cast<node*>(DRYAD_CTHIS->parent());
    }

    //=== color ===//
    dryad::color color() const
    {
        return dryad::color((_ptr & 0b110) >> 1);
    }
    void set_color(dryad::color color)
    {
        _ptr &= ~0b110;
        _ptr |= (unsigned(color) & 0b11) << 1;
    }

protected:
    using base_node = node;

    //=== construction/destruction ===//
    explicit node(node_ctor, NodeKind kind)
    : _ptr(reinterpret_cast<std::uintptr_t>((node*)nullptr)), _is_container(false),
      _kind(_traits::to_int(kind)), _user_data16(0), _user_data32(0)
    {}

    node(const node&)            = delete;
    node& operator=(const node&) = delete;

    ~node() = default;

    //=== user data ===//
    std::uint16_t& user_data16()
    {
        return _user_data16;
    }
    std::uint32_t& user_data32()
    {
        return _user_data32;
    }

    std::uint16_t user_data16() const
    {
        return _user_data16;
    }
    std::uint32_t user_data32() const
    {
        return _user_data32;
    }

private:
    node_container<NodeKind>* next_container() const
    {
        DYRAD_PRECONDITION(next_node()->_is_container);
        return static_cast<node_container<NodeKind>*>(next_node());
    }
    bool next_is_parent() const
    {
        return (_ptr & 0b1) != 0;
    }

    void set_next_sibling(node* node)
    {
        _ptr = reinterpret_cast<std::uintptr_t>(node);
        DRYAD_ASSERT((_ptr & 0b111) == 0, "invalid pointer alignment");
    }
    void set_next_parent(node* node)
    {
        _ptr = reinterpret_cast<std::uintptr_t>(node);
        DRYAD_ASSERT((_ptr & 0b111) == 0, "invalid pointer alignment");
        _ptr |= 0b1;
    }
    void copy_next(node* copy_from)
    {
        _ptr = copy_from->_ptr;
    }

    // If part of a tree, ptr points to either sibling or parent.
    // For root node, it is a parent pointer to itself.
    // If not part of a tree, ptr is nullptr.
    //
    // 3 bits for alignment: cct
    // * cc: color
    // * t: 0 if sibling, 1 if parent
    std::uintptr_t _ptr;
    bool           _is_container : 1;
    std::uint16_t  _kind : 15;
    std::uint16_t  _user_data16;
    std::uint32_t  _user_data32;

    friend node_container<NodeKind>;
};

/// Base class for all nodes that own child nodes, which should be traversed.
template <typename NodeKind>
class node_container : public node<NodeKind>
{
protected:
    using base_node = node_container;

    explicit node_container(node_ctor ctor, NodeKind kind) : node<NodeKind>(ctor, kind)
    {
        this->_is_container = true;
    }

    ~node_container() = default;

    node<NodeKind>* first_child() const
    {
        return _first_child;
    }

    void insert_first_child(node<NodeKind>* child)
    {
        DRYAD_PRECONDITION(_first_child == nullptr);
        child->set_next_parent(this);
        _first_child = child;
    }
    void insert_child_front(node<NodeKind>* child)
    {
        DRYAD_PRECONDITION(_first_child != nullptr);
        child->set_next_sibling(_first_child);
        _first_child = child;
    }
    void insert_child_after(node<NodeKind>* pos, node<NodeKind>* child)
    {
        DRYAD_PRECONDITION(_first_child != nullptr);
        child->copy_next(pos);
        pos->set_next_sibling(child);
    }

private:
    node<NodeKind>* _first_child;
};
} // namespace dryad

#endif // DRYAD_NODE_HPP_INCLUDED

