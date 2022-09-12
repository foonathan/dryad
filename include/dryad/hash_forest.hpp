// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_HASH_FOREST_HPP_INCLUDED
#define DRYAD_HASH_FOREST_HPP_INCLUDED

#include <cstring>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/hash_table.hpp>
#include <dryad/arena.hpp>
#include <dryad/hash_algorithm.hpp>
#include <dryad/node.hpp>
#include <dryad/tree.hpp>

#if 0
struct NodeHasher : ndryad::node_hasher_base<NodeHasher, NodeType1, NodeType2, ...>
{
    // Hashes the non-tree data of each node type.
    // Does not need to account for children or the node kind.
    template <typename HashAlgorithm>
    static void hash(HashAlgorithm& hasher, const NodeType1* n);
    template <typename HashAlgorithm>
    static void hash(HashAlgorithm& hasher, const NodeType2* n);
    ...

    template <typename HashAlgorithm>
    static void hash(HashAlgorithm& hasher, Key key);

    // Compares the non-tree data of each node type.
    // Does not need to compare children.
    static bool is_equal(const NodeType1* lhs, const NodeType1* rhs);
    static bool is_equal(const NodeType2* lhs, const NodeType2* rhs);
    ...

    static bool is_equal(const NodeTypeN* value, Key key);
    ...
};
#endif

namespace dryad
{
template <typename Derived, typename... NodeTypes>
struct node_hasher_base
{
    template <typename HashAlgorithm, typename NodeType>
    struct hasher_for
    {
        HashAlgorithm& hasher;

        void operator()(const NodeType* n) const
        {
            Derived::hash(hasher, n);
        }
    };

    template <typename HashAlgorithm, typename Node>
    static void hash_base(HashAlgorithm& hasher, const Node* n)
    {
        hasher.hash_scalar(n->kind());
        visit_tree(n, hasher_for<HashAlgorithm, NodeTypes>{hasher}...);
    }

    template <typename Node, typename NodeType>
    struct is_equal_for
    {
        const Node* rhs;

        bool operator()(const NodeType* lhs_casted) const
        {
            auto rhs_casted = node_cast<NodeType>(rhs);
            return Derived::is_equal(lhs_casted, rhs_casted);
        }
    };

    template <typename Node>
    static bool is_equal_base(const Node* lhs, const Node* rhs)
    {
        // Compare kind.
        if (lhs->kind() != rhs->kind())
            return false;

        // Compare node data itself.
        if (!visit_node_all(lhs, is_equal_for<Node, NodeTypes>{rhs}...))
            return false;

        // Compare children.
        auto lhs_cur = lhs->children().begin();
        auto lhs_end = lhs->children().end();
        auto rhs_cur = rhs->children().begin();
        auto rhs_end = rhs->children().end();
        while (lhs_cur != lhs_end && rhs_cur != rhs_end)
        {
            if (!is_equal_base(*lhs_cur, *rhs_cur))
                return false;

            ++lhs_cur;
            ++rhs_cur;
        }
        return lhs_cur == lhs_end && rhs_cur == rhs_end;
    }
};
} // namespace dryad

namespace dryad::_detail
{
template <typename T, typename Key>
struct hash_forest_key
{
    const Key* key;
};

template <typename RootNode, typename NodeHasher>
struct hash_forest_traits
{
    using value_type = RootNode*;

    static bool is_unoccupied(value_type ptr)
    {
        auto bits = reinterpret_cast<std::uintptr_t>(ptr);
        return bits == 0 || bits == std::uintptr_t(-1);
    }

    static void fill_removed(value_type* data, size_t size)
    {
        std::memset(data, static_cast<unsigned char>(-1), size * sizeof(value_type));
    }

    static void fill_unoccupied(value_type* data, size_t size)
    {
        std::memset(data, static_cast<unsigned char>(0), size * sizeof(value_type));
    }

    static bool is_equal(value_type entry, value_type key)
    {
        return NodeHasher::is_equal_base(entry, key);
    }
    template <typename T, typename Key>
    static bool is_equal(value_type entry, hash_forest_key<T, Key> key)
    {
        if (auto node = node_try_cast<T>(entry))
            return NodeHasher::is_equal(node, *key.key);
        else
            return false;
    }

    static std::size_t hash(value_type entry)
    {
        default_hash_algorithm hasher;
        NodeHasher::hash_base(hasher, entry);
        return DRYAD_MOV(hasher).finish();
    }
    template <typename T, typename Key>
    static std::size_t hash(hash_forest_key<T, Key> key)
    {
        default_hash_algorithm hasher;
        hasher.hash_scalar(T::kind());
        NodeHasher::hash(hasher, *key.key);
        return DRYAD_MOV(hasher).finish();
    }
};
} // namespace dryad::_detail

namespace dryad
{
/// Owns multiple nodes, eventually all children of a list of root nodes.
/// Identical trees are interned.
template <typename RootNode, typename NodeHasher, typename MemoryResource = void>
class hash_forest
{
    using hash_table = _detail::hash_table<_detail::hash_forest_traits<RootNode, NodeHasher>, 64>;

public:
    using node_kind_type = typename RootNode::node_kind_type;

    //=== construction ===//
    hash_forest() = default;
    explicit hash_forest(MemoryResource* resource) : _arena(resource) {}

    ~hash_forest()
    {
        _roots.free(_arena.resource());
    }

    hash_forest(hash_forest&& other) noexcept : _arena(LEXY_MOV(other._arena)), _roots(other._roots)
    {
        other._roots = {};
    }
    hash_forest& operator=(hash_forest&& other) noexcept
    {
        _detail::swap(_arena, other._arena);
        _detail::swap(_roots, other._roots);
        return *this;
    }

    //=== node creation ===//
    class node_creator
    {
    public:
        /// Creates a node of type T that is not yet linked into the tree.
        /// The pointer must not be used outside the creation function.
        template <typename T, typename... Args>
        T* create(Args&&... args)
        {
            static_assert(is_node<T, node_kind_type> && !is_abstract_node<T, node_kind_type>);
            static_assert(std::is_trivially_destructible_v<T>, "nobody will call its destructor");

            return _forest->_arena.template construct<T>(node_ctor{}, DRYAD_FWD(args)...);
        }

    private:
        explicit node_creator(hash_forest* forest) : _forest(forest) {}

        hash_forest* _forest;

        friend hash_forest;
    };

    template <typename Builder>
    RootNode* build(Builder builder)
    {
        if (_roots.should_rehash())
            rehash(2 * _roots.capacity());

        auto marker = _arena.top();

        auto node = builder(node_creator(this));
        node->set_next_parent(node);

        auto entry = _roots.lookup_entry(node);
        if (entry)
        {
            // Node is already part of the tree, remove its memory and return existing one.
            _arena.unwind(marker);
            return entry.get();
        }
        else
        {
            // Node is not part of the tree.
            entry.create(node);
            return node;
        }
    }

    template <typename T, typename... Args>
    RootNode* create(Args&&... args)
    {
        return build(
            [&](node_creator creator) { return creator.template create<T>(DRYAD_FWD(args)...); });
    }

    template <typename T, typename Key>
    RootNode* lookup_or_create(Key&& key)
    {
        if (_roots.should_rehash())
            rehash(2 * _roots.capacity());

        auto entry = _roots.lookup_entry(_detail::hash_forest_key<T, Key>{&key});
        if (entry)
            return entry.get();

        auto node = node_creator(this).template create<T>(DRYAD_FWD(key));
        entry.create(node);
        return node;
    }

    void clear()
    {
        _roots.free(_arena.resource());
        _arena.clear();
    }

    //=== hash table interface ===//
    void rehash(std::size_t new_capacity)
    {
        new_capacity  = _roots.to_table_capacity(new_capacity);
        auto capacity = _roots.capacity();
        if (new_capacity <= capacity)
            return;

        _roots.rehash(_arena.resource(), new_capacity);
    }

    std::size_t root_count() const
    {
        return _roots.size();
    }
    std::size_t root_capacity() const
    {
        return _roots.capacity();
    }

private:
    arena<MemoryResource> _arena;
    hash_table            _roots;
};
} // namespace dryad

#endif // DRYAD_HASH_FOREST_HPP_INCLUDED

