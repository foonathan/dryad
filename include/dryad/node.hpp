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

enum class traverse_event;
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
        const node* _self;

        struct iterator : _detail::forward_iterator_base<iterator, T*, T*, void>
        {
            T* _cur = nullptr;

            static iterator from_ptr(T* ptr)
            {
                return {ptr};
            }

            operator typename _sibling_range<const T>::iterator() const
            {
                return {_cur};
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
                    DRYAD_ASSERT(!_cur->next_node()->children().empty(), "parent node is empty?!");
                    _cur = *_cur->next_node()->children().begin();
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

        T* front() const
        {
            return *begin();
        }
    };

    _sibling_range<node<NodeKind>> siblings()
    {
        return {this};
    }
    _sibling_range<const node<NodeKind>> siblings() const
    {
        return {this};
    }

    template <typename T>
    struct _children_range
    {
        const node* _self;

        struct iterator : _detail::forward_iterator_base<iterator, T*, T*, void>
        {
            T* _cur = nullptr;

            static iterator from_ptr(T* ptr)
            {
                return {ptr};
            }

            operator typename _children_range<const T>::iterator() const
            {
                return {_cur};
            }

            T* deref() const
            {
                return (T*)_cur;
            }
            void increment()
            {
                _cur = _cur->next_node();
            }
            bool equal(iterator rhs) const
            {
                return _cur == rhs._cur;
            }
        };

        bool empty() const
        {
            return _self->_is_container ? _self->_user_data_ptr == nullptr : true;
        }

        iterator begin() const
        {
            if (empty())
                return {{}, nullptr};
            else
                return {{}, static_cast<T*>(_self->_user_data_ptr)};
        }
        iterator end() const
        {
            if (empty())
                // begin() == nullptr, so return that as well.
                return {{}, nullptr};
            else
                // The last child has a pointer back to self.
                return {{}, const_cast<T*>(_self)};
        }

        T* front() const
        {
            return *begin();
        }
    };

    _children_range<node> children()
    {
        return {this};
    }
    _children_range<const node> children() const
    {
        return {this};
    }

    bool has_children() const
    {
        return !children().empty();
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
    void unlink()
    {
        _ptr = reinterpret_cast<std::uintptr_t>((node*)nullptr);
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

    node* next_node() const
    {
        return reinterpret_cast<node*>(_ptr & ~std::uintptr_t(0b111));
    }
    bool next_node_is_parent() const
    {
        return (_ptr & 0b1) != 0;
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

    template <typename>
    friend class container_node;
    template <typename, typename>
    friend class tree;
    template <typename>
    friend class _traverse_range;
};
} // namespace dryad

namespace dryad
{
/// A node without any special members.
template <auto NodeKind, typename AbstractBase = node<DRYAD_DECAY_DECLTYPE(NodeKind)>>
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

    template <typename... Args>
    explicit basic_node(node_ctor ctor, Args&&... args)
    : AbstractBase(ctor, kind(), DRYAD_FWD(args)...)
    {}

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
/// Base class for all nodes that own child nodes, which should be traversed.
template <typename AbstractBase>
class container_node : public AbstractBase
{
    using _node = node<typename AbstractBase::node_kind_type>;

public:
    using node_kind_type = typename AbstractBase::node_kind_type;

protected:
    explicit container_node(node_ctor ctor, node_kind_type kind) : AbstractBase(ctor, kind)
    {
        this->_is_container = true;
    }
    ~container_node() = default;

    //=== modifiers ===//
    void insert_child_after(_node* pos, _node* child)
    {
        DRYAD_PRECONDITION(!child->is_linked_in_tree());

        if (pos == nullptr)
        {
            if (auto first = first_child())
                child->set_next_sibling(first);
            else
                child->set_next_parent(this);

            set_first_child(child);
        }
        else
        {
            child->copy_next(pos);
            pos->set_next_sibling(child);
        }
    }
    template <typename... T>
    void insert_children_after(_node* pos, T*... children)
    {
        ((insert_child_after(pos, children), pos = children), ...);
    }

    /// Returns the child that was erased.
    _node* erase_child_after(_node* pos)
    {
        if (pos == nullptr)
        {
            DRYAD_PRECONDITION(first_child() != nullptr);
            auto child = first_child();

            if (child->next_node_is_parent())
                set_first_child(nullptr);
            else
                set_first_child(child->next_node());

            child->unlink();
            return child;
        }
        else
        {
            DRYAD_PRECONDITION(!pos->next_node_is_parent());
            auto child = pos->next_node();

            pos->copy_next(child);

            child->unlink();
            return child;
        }
    }

    /// Returns the previous child.
    _node* replace_child_after(_node* pos, _node* new_child)
    {
        DRYAD_PRECONDITION(!new_child->is_linked_in_tree());

        if (pos == nullptr)
        {
            DRYAD_PRECONDITION(first_child());
            auto old_child = first_child();

            new_child->copy_next(old_child);
            set_first_child(new_child);

            old_child->unlink();
            return old_child;
        }
        else
        {
            DRYAD_PRECONDITION(!pos->next_node_is_parent());
            auto old_child = pos->next_node();

            new_child->copy_next(old_child);
            pos->set_next_sibling(new_child);

            old_child->unlink();
            return old_child;
        }
    }

private:
    _node* first_child() const
    {
        return static_cast<_node*>(this->user_data_ptr());
    }
    void set_first_child(_node* new_child)
    {
        this->user_data_ptr() = new_child;
    }
    using _node::user_data_ptr;

    friend _node;
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

namespace dryad
{
template <typename NodeIterator, typename NodeType = typename NodeIterator::value_type>
struct node_range
{
    NodeIterator _begin;
    NodeIterator _end;

    node_range(NodeIterator begin, NodeIterator end) : _begin(begin), _end(end) {}

    struct iterator : _detail::forward_iterator_base<iterator, NodeType*, NodeType*, void>
    {
        NodeIterator _cur;

        NodeType* deref() const
        {
            return node_cast<NodeType>(*_cur);
        }
        void increment()
        {
            ++_cur;
        }
        bool equal(iterator rhs) const
        {
            return _cur == rhs._cur;
        }
    };

    bool empty() const
    {
        return _begin == _end;
    }

    iterator begin() const
    {
        return {{}, _begin};
    }
    iterator end() const
    {
        return {{}, _end};
    }

    NodeType* front() const
    {
        return *begin();
    }
};

template <typename NodeIterator>
auto make_node_range(NodeIterator begin, NodeIterator end)
{
    return node_range<NodeIterator>(begin, end);
}
template <typename NodeType, typename NodeIterator>
auto make_node_range(NodeIterator begin, NodeIterator end)
{
    return node_range<NodeIterator, NodeType>(begin, end);
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
    using type = typename tail::template insert<T>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(T*) const, auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<T>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(traverse_event, T*), auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<T>;
};
template <typename Lambda, typename T, void (Lambda::*Ptr)(traverse_event, T*) const, auto... Tail>
struct _node_types_for_lambdas<Ptr, Tail...>
{
    using tail = typename _node_types_for_lambdas<Tail...>::type;
    using type = typename tail::template insert<T>;
};

template <typename... Lambdas>
using node_types_for_lambdas = typename _node_types_for_lambdas<&Lambdas::operator()...>::type;
} // namespace dryad::_detail

namespace dryad
{
template <typename NodeTypeList, typename... Lambdas>
struct _visit_node;
template <typename... NodeTypes, typename... Lambdas>
struct _visit_node<_detail::node_type_list<NodeTypes...>, Lambdas...>
{
    template <bool All = false, typename Node>
    DRYAD_FORCE_INLINE static void visit(Node* node, Lambdas&&... lambdas)
    {
        auto                  kind = node->kind();
        [[maybe_unused]] auto found_callback
            = ((NodeTypes::type_matches_kind(kind) ? (lambdas(static_cast<NodeTypes*>(node)), true)
                                                   : false)
               || ...);

        if constexpr (All)
            DRYAD_ASSERT(found_callback, "missing type for callback");
    }
};

/// Visits the node invoking the appropriate lambda for each node type.
///
/// It will try each lambda in the order specified, NodeType can be abstract in which case it
/// swallows all. Only one lambda will be invoked. If the type of a node does not match any lambda,
/// it will not be invoked.
template <typename NodeKind, typename... Lambdas>
void visit_node(node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_node<node_types, Lambdas...>::visit(node, DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename... Lambdas>
void visit_node(const node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_node<node_types, Lambdas...>::visit(node, DRYAD_FWD(lambdas)...);
}

/// Same as above, but it is an error if a node cannot be visited.
template <typename NodeKind, typename... Lambdas>
void visit_node_all(node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_node<node_types, Lambdas...>::template visit<true>(node, DRYAD_FWD(lambdas)...);
}
template <typename NodeKind, typename... Lambdas>
void visit_node_all(const node<NodeKind>* node, Lambdas&&... lambdas)
{
    using node_types = _detail::node_types_for_lambdas<std::decay_t<Lambdas>...>;
    _visit_node<node_types, Lambdas...>::template visit<true>(node, DRYAD_FWD(lambdas)...);
}
} // namespace dryad

#endif // DRYAD_NODE_HPP_INCLUDED

