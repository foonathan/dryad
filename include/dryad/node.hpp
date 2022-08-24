// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_NODE_HPP_INCLUDED
#define DRYAD_NODE_HPP_INCLUDED

#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/iterator.hpp>

namespace dryad
{
template <typename NodeKind>
class node;
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
template <typename T, typename NodeKind>
constexpr bool is_abstract_node = is_node<T, NodeKind>&& T::type_is_abstract();
} // namespace dryad

namespace dryad
{
class node_ctor
{
    node_ctor() = default;

    template <typename, typename>
    friend class tree;
};

/// Type-erased base class for all nodes in the AST.
///
/// User nodes should never inherit from it directly.
template <typename NodeKind>
class node
{
    using _traits = node_kind_traits<NodeKind>;

public:
    node(const node&)            = delete;
    node& operator=(const node&) = delete;

    using node_kind_type = NodeKind;

    static constexpr bool type_is_abstract()
    {
        return true;
    }
    static constexpr bool type_matches_kind(NodeKind)
    {
        return true;
    }

    NodeKind kind() const
    {
        return _traits::from_int(_kind);
    }

    //=== tree relationship ===//
    bool is_linked_in_tree() const
    {
        return next_node() != nullptr;
    }

    node* next_node()
    {
        return reinterpret_cast<node*>(_ptr & ~std::uintptr_t(0b111));
    }
    const node* next_node() const
    {
        return reinterpret_cast<const node*>(_ptr & ~std::uintptr_t(0b111));
    }

    bool next_node_is_parent() const
    {
        return (_ptr & 0b1) != 0;
    }

    node<NodeKind>* first_child()
    {
        return _is_container ? static_cast<node<NodeKind>*>(_user_data_ptr) : nullptr;
    }
    const node<NodeKind>* first_child() const
    {
        return _is_container ? static_cast<const node<NodeKind>*>(_user_data_ptr) : nullptr;
    }

    /// Root node returns a pointer to itself.
    const node* parent() const
    {
        if (!is_linked_in_tree())
            return nullptr;

        // If we follow the sibling pointer repeatedly, we end up at the parent eventually.
        auto cur = this;
        while (!cur->next_node_is_parent())
            cur = cur->next_node();
        return cur->next_node();
    }
    node* parent()
    {
        return const_cast<node*>(DRYAD_CTHIS->parent());
    }

    template <typename T>
    struct _sibling_range
    {
        struct iterator : _detail::forward_iterator_base<iterator, T*, T*, void>
        {
            T* _cur = nullptr;

            operator typename _sibling_range<const T>::iterator() const
            {
                return {_cur};
            }

            operator T*() const
            {
                return _cur;
            }
            T* deref() const
            {
                return _cur;
            }
            void increment()
            {
                if (_cur->next_node_is_parent())
                {
                    // We're pointing to the parent, go to first child instead.
                    // As it's a container, this is the ptr user data.
                    DRYAD_ASSERT(_cur->next_node()->is_container(),
                                 "parent node is not a container?!");
                    _cur = _cur->next_node()->first_child();
                }
                else
                    // We're pointing to a sibling, go there.
                    _cur = _cur->next_node();
            }
            bool equal(iterator rhs) const
            {
                return _cur == rhs._cur;
            }
        };

        bool empty() const
        {
            return begin() == end();
        }

        iterator begin() const
        {
            if (!_self->is_linked_in_tree() || _self->next_node() == _self)
                return {};

            // We begin with the next node after ours.
            // If we don't have siblings, this is our node itself.
            return ++end();
        }
        iterator end() const
        {
            if (!_self->is_linked_in_tree() || _self->next_node() == _self)
                return {};

            // We end when we're back at the node.
            return {{}, (node<NodeKind>*)_self};
        }

        const node* _self;
    };

    _sibling_range<node<NodeKind>> siblings()
    {
        return {this};
    }
    _sibling_range<const node<NodeKind>> siblings() const
    {
        return {this};
    }

    bool is_container() const
    {
        return _is_container;
    }

    //=== color ===//
    dryad::color color() const
    {
        return dryad::color((_ptr & 0b110) >> 1);
    }
    void set_color(dryad::color color)
    {
        _ptr &= ~std::uintptr_t(0b110);
        _ptr |= (unsigned(color) & 0b11) << 1;
    }

protected:
    //=== user data ===//
    std::uint16_t& user_data16()
    {
        return _user_data16;
    }
    std::uint32_t& user_data32()
    {
        return _user_data32;
    }
    void*& user_data_ptr()
    {
        return _user_data_ptr;
    }

    std::uint16_t user_data16() const
    {
        return _user_data16;
    }
    std::uint32_t user_data32() const
    {
        return _user_data32;
    }
    void* user_data_ptr() const
    {
        return _user_data_ptr;
    }

protected:
    explicit node(node_ctor, NodeKind kind)
    : _ptr(reinterpret_cast<std::uintptr_t>((node*)nullptr)), _is_container(false),
      _kind(_traits::to_int(kind) & 0x7FFF), _user_data16(0), _user_data32(0),
      _user_data_ptr(nullptr)
    {}
    ~node() = default;

private:
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
    void*          _user_data_ptr; // If it's a container, pointer to first child.

    template <typename, typename>
    friend class _container_node;
    template <typename, typename>
    friend class tree;
};
} // namespace dryad

namespace dryad
{
/// A node without any special members.
template <typename AbstractBase, auto NodeKind>
class basic_node : public AbstractBase
{
    static_assert(is_abstract_node<AbstractBase, DRYAD_DECAY_DECLTYPE(NodeKind)>);
    static_assert(AbstractBase::type_matches_kind(NodeKind));

public:
    static constexpr bool type_is_abstract()
    {
        return false;
    }
    static constexpr bool type_matches_kind(DRYAD_DECAY_DECLTYPE(NodeKind) kind)
    {
        return kind == NodeKind;
    }

    static constexpr auto kind()
    {
        return NodeKind;
    }

protected:
    using node_base = basic_node;
    explicit basic_node(node_ctor ctor) : AbstractBase(ctor, kind()) {}
    ~basic_node() = default;
};

#define DRYAD_NODE_CTOR(Name)                                                                      \
    explicit Name(::dryad::node_ctor ctor) : node_base(ctor) {}

#define DRYAD_ATTRIBUTE_USER_DATA16(T, Name)                                                       \
    static_assert(sizeof(T) <= sizeof(std::uint16_t));                                             \
    T Name() const                                                                                 \
    {                                                                                              \
        return static_cast<T>(node_base::user_data16());                                           \
    }                                                                                              \
    void set_##Name(T val)                                                                         \
    {                                                                                              \
        node_base::user_data16() = static_cast<std::uint16_t>(val);                                \
    }                                                                                              \
    void user_data16() = delete

#define DRYAD_ATTRIBUTE_USER_DATA32(T, Name)                                                       \
    static_assert(sizeof(T) <= sizeof(std::uint32_t));                                             \
    T Name() const                                                                                 \
    {                                                                                              \
        return static_cast<T>(node_base::user_data32());                                           \
    }                                                                                              \
    void set_##Name(T val)                                                                         \
    {                                                                                              \
        node_base::user_data32() = static_cast<std::uint32_t>(val);                                \
    }                                                                                              \
    void user_data32() = delete

#define DRYAD_ATTRIBUTE_USER_DATA_PTR(T, Name)                                                     \
    static_assert(sizeof(T) <= sizeof(void*));                                                     \
    T Name() const                                                                                 \
    {                                                                                              \
        return (T)(node_base::user_data_ptr());                                                    \
    }                                                                                              \
    void set_##Name(T val)                                                                         \
    {                                                                                              \
        node_base::user_data_ptr() = (void*)(val);                                                 \
    }                                                                                              \
    void user_data_ptr() = delete
} // namespace dryad

namespace dryad
{
// We use a helper class that is partially type-erased to remove template bloat.
template <typename AbstractBase, typename NodeKind>
class _container_node : public AbstractBase
{
protected:
    void insert_first_child(node<NodeKind>* child)
    {
        DRYAD_PRECONDITION(!child->is_linked_in_tree());
        DRYAD_PRECONDITION(this->first_child() == nullptr);
        child->set_next_parent(this);
        this->user_data_ptr() = child;
    }
    void insert_child_front(node<NodeKind>* child)
    {
        DRYAD_PRECONDITION(!child->is_linked_in_tree());
        DRYAD_PRECONDITION(this->first_child() != nullptr);
        child->set_next_sibling(this->first_child());
        this->user_data_ptr() = child;
    }
    void insert_child_after(node<NodeKind>* pos, node<NodeKind>* child)
    {
        DRYAD_PRECONDITION(!child->is_linked_in_tree());
        DRYAD_PRECONDITION(this->first_child() != nullptr);
        child->copy_next(pos);
        pos->set_next_sibling(child);
    }

    node<NodeKind>* erase_child_after(node<NodeKind>* pos)
    {
        if (pos == nullptr)
        {
            auto child = this->first_child();

            if (child->next_node_is_parent())
                this->user_data_ptr() = nullptr;
            else
                this->user_data_ptr() = child->next_node();

            child->set_next_sibling(nullptr);
            return child;
        }
        else
        {
            DRYAD_PRECONDITION(!pos->next_node_is_parent());
            auto child = pos->next_node();
            pos->copy_next(child);
            child->set_next_sibling(nullptr);
            return child;
        }
    }

private:
    explicit _container_node(node_ctor ctor, NodeKind kind) : AbstractBase(ctor, kind)
    {
        this->_is_container = true;
    }

    ~_container_node() = default;

    using node<NodeKind>::user_data_ptr;

    friend node<NodeKind>;
    template <typename, auto>
    friend class basic_container_node;
};

/// Base class for all nodes that own child nodes, which should be traversed.
/// This is just an implementation detail that is not relevant unless you implement your own
/// containers.
template <typename AbstractBase, auto NodeKind>
class basic_container_node : public _container_node<AbstractBase, DRYAD_DECAY_DECLTYPE(NodeKind)>
{
    static_assert(is_abstract_node<AbstractBase, DRYAD_DECAY_DECLTYPE(NodeKind)>);
    static_assert(AbstractBase::type_matches_kind(NodeKind));

public:
    static constexpr bool type_is_abstract()
    {
        return false;
    }
    static constexpr bool type_matches_kind(DRYAD_DECAY_DECLTYPE(NodeKind) kind)
    {
        return kind == NodeKind;
    }

    static constexpr auto kind()
    {
        return NodeKind;
    }

protected:
    using node_base = basic_container_node;
    explicit basic_container_node(node_ctor ctor)
    : _container_node<AbstractBase, DRYAD_DECAY_DECLTYPE(NodeKind)>(ctor, kind())
    {}
    ~basic_container_node() = default;
};
} // namespace dryad

namespace dryad
{
template <typename T, typename NodeKind>
bool node_has_kind(const node<NodeKind>* ptr)
{
    static_assert(is_node<T, NodeKind>);
    return T::type_matches_kind(ptr->kind());
}

template <typename T, typename NodeKind>
T* node_cast(node<NodeKind>* ptr)
{
    static_assert(is_node<T, NodeKind>);
    DRYAD_PRECONDITION(node_has_kind<T>(ptr));
    return static_cast<T*>(ptr);
}
template <typename T, typename NodeKind>
const T* node_cast(const node<NodeKind>* ptr)
{
    static_assert(is_node<T, NodeKind>);
    DRYAD_PRECONDITION(node_has_kind<T>(ptr));
    return static_cast<const T*>(ptr);
}

template <typename T, typename NodeKind>
T* node_try_cast(node<NodeKind>* ptr)
{
    static_assert(is_node<T, NodeKind>);
    if (node_has_kind<T>(ptr))
        return static_cast<T*>(ptr);
    else
        return nullptr;
}
template <typename T, typename NodeKind>
const T* node_try_cast(const node<NodeKind>* ptr)
{
    static_assert(is_node<T, NodeKind>);
    if (node_has_kind<T>(ptr))
        return static_cast<const T*>(ptr);
    else
        return nullptr;
}
} // namespace dryad

#endif // DRYAD_NODE_HPP_INCLUDED

