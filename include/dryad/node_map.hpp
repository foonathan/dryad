// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_NODE_MAP_HPP_INCLUDED
#define DRYAD_NODE_MAP_HPP_INCLUDED

#include <cstring>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/hash_table.hpp>
#include <dryad/node.hpp>

namespace dryad::_detail
{
template <typename NodeType>
struct node_hash_traits
{
    using value_type = NodeType*;

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

    static bool is_equal(value_type entry, std::remove_const_t<NodeType>* value)
    {
        return entry == value;
    }
    static bool is_equal(value_type entry, const NodeType* key)
    {
        return entry == key;
    }

    static std::size_t hash(std::remove_const_t<NodeType>* value)
    {
        auto bits = reinterpret_cast<std::uintptr_t>(value);
        // We just remove the alignment bits (which are always zero), and use the rest as-is.
        // If benchmarks show bad performance, we might reconsider it.
        return bits >> 3;
    }
    static std::size_t hash(const NodeType* key)
    {
        return hash(const_cast<std::remove_const_t<NodeType>*>(key));
    }
};
} // namespace dryad::_detail

namespace dryad
{
struct _node_map_void
{};

/// Maps pointers to `NodeType` (which may be `const`) to `Value` (which may be `void`).
template <typename NodeType, typename Value, typename MemoryResource = void>
class node_map
{
    using resource_ptr = _detail::memory_resource_ptr<MemoryResource>;
    using traits       = _detail::node_hash_traits<NodeType>;
    using hash_table   = _detail::hash_table<traits, 64>;

    using value_array = std::conditional_t<std::is_void_v<Value>, _node_map_void, Value*>;

public:
    using key_type    = NodeType*;
    using mapped_type = Value;

    //=== constructors ===//
    constexpr node_map() : _values(), _resource(_detail::get_memory_resource<MemoryResource>()) {}
    constexpr explicit node_map(MemoryResource* resource) : _values(), _resource(resource) {}

    ~node_map()
    {
        if constexpr (!std::is_void_v<Value> && !std::is_trivially_destructible_v<Value>)
        {
            for (auto entry : _table.entries())
                _values[entry.index()].~Value();
        }
        if constexpr (!std::is_void_v<Value>)
            _resource->deallocate(_values, _table.capacity() * sizeof(Value), alignof(Value));

        _table.free(_resource);
    }

    node_map(node_map&& other) noexcept
    : _table(other._table), _values(other._values), _resource(other._resource)
    {
        other._table  = {};
        other._values = {};
    }

    node_map& operator=(node_map&& other) noexcept
    {
        _detail::swap(_table, other._table);
        _detail::swap(_values, other._values);
        _detail::swap(_resource, other._resource);
        return *this;
    }

    //=== access ===//
    bool empty() const
    {
        return _table.size() == 0;
    }
    std::size_t size() const
    {
        return _table.size();
    }
    std::size_t capacity() const
    {
        return _table.capacity();
    }

    //=== entry_handle ===//
    class entry_handle
    {
    public:
        explicit operator bool() const
        {
            return static_cast<bool>(_entry);
        }

        NodeType* node() const
        {
            return _key;
        }

        decltype(auto) value() const
        {
            DRYAD_PRECONDITION(*this);
            return _values[_entry.index()];
        }

        template <typename... Args>
        decltype(auto) insert(Args&&... args)
        {
            DRYAD_PRECONDITION(!*this);
            _entry.create(_key);
            if constexpr (!std::is_void_v<Value>)
                return *::new (&_values[_entry.index()]) Value(DRYAD_FWD(args)...);
        }

        template <typename... Args>
        decltype(auto) update(Args&&... args) const
        {
            DRYAD_PRECONDITION(*this);
            return _values[_entry.index()] = Value(DRYAD_FWD(args)...);
        }

        void remove()
        {
            DRYAD_PRECONDITION(*this);
            if constexpr (!std::is_void_v<Value>)
                _values[_entry.index()].~Value();
            _entry.remove();
        }

        typename hash_table::entry_handle _entry;
        DRYAD_EMPTY_MEMBER value_array    _values;
        NodeType*                         _key;
    };

    //=== modifiers ===//
    void rehash(std::size_t new_capacity)
    {
        new_capacity = _table.to_table_capacity(new_capacity);
        if (new_capacity <= _table.capacity())
            return;

        if constexpr (std::is_void_v<Value>)
        {
            _table.rehash(_resource, new_capacity, traits{});
        }
        else
        {
            auto new_values = static_cast<Value*>(
                _resource->allocate(new_capacity * sizeof(Value), alignof(Value)));

            _table.rehash(_resource, new_capacity, traits{}, [&](auto new_entry, auto old_idx) {
                // Do a destructive move of the value to the new array.
                auto new_idx = new_entry.index();
                ::new (&new_values[new_idx]) Value(DRYAD_MOV(_values[old_idx]));
                _values[old_idx].~Value();
            });
            _values = new_values;
        }
    }

    entry_handle lookup_entry(NodeType* node)
    {
        if (_table.should_rehash())
            rehash(2 * _table.capacity());

        return {_table.lookup_entry(node, traits{}), _values, node};
    }

    bool contains(const NodeType* node)
    {
        if (empty())
            return false;

        auto entry = _table.lookup_entry(node, traits{});
        return static_cast<bool>(entry);
    }

    Value* lookup(const NodeType* node)
    {
        if (empty())
            return nullptr;

        auto entry = _table.lookup_entry(node, traits{});
        return entry ? &_values[entry.index()] : nullptr;
    }
    const Value* lookup(const NodeType* node) const
    {
        return const_cast<node_map*>(this)->lookup(node);
    }

    template <typename... Args>
    bool insert(NodeType* node, Args&&... args)
    {
        auto entry = lookup_entry(node);
        if (entry)
            return false;

        entry.insert(DRYAD_FWD(args)...);
        return true;
    }
    template <typename... Args>
    bool insert_or_update(NodeType* node, Args&&... args)
    {
        auto entry = lookup_entry(node);
        if (entry)
        {
            entry.update(DRYAD_FWD(args)...);
            return false;
        }
        else
        {
            entry.insert(DRYAD_FWD(args)...);
            return true;
        }
    }

    bool remove(const NodeType* node)
    {
        auto entry = lookup_entry(const_cast<NodeType*>(node));
        if (!entry)
            return false;

        entry.remove();
        return true;
    }

private:
    hash_table                      _table;
    DRYAD_EMPTY_MEMBER value_array  _values; // shares key index
    DRYAD_EMPTY_MEMBER resource_ptr _resource;
};

template <typename NodeType, typename MemoryResource = void>
using node_set = node_map<NodeType, void, MemoryResource>;
} // namespace dryad

#endif // DRYAD_NODE_MAP_HPP_INCLUDED

