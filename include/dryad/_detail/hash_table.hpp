// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_DETAIL_HASH_TABLE_HPP_INCLUDED
#define DRYAD_DETAIL_HASH_TABLE_HPP_INCLUDED

#include <climits>
#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/iterator.hpp>
#include <dryad/_detail/memory_resource.hpp>

#if 0
struct Traits
{
    using value_type = T;

    static bool is_unoccupied(const T&);
    static void fill_removed(T* data, size_t size);
    static void fill_unoccupied(T* data, size_t size);

    bool is_equal(const T& entry, const T& value) const;
    template <typename Key>
    bool is_equal(const T& entry, const Key& key) const;
    
    std::size_t hash(const T& value) const;
    template <typename Key>
    std::size_t hash(const Key& key) const;
};
#endif

namespace dryad::_detail
{
/// A simple hash table for trivial keys with linear probing.
/// It is non-owning as it does not store the used memory resource.
template <typename Traits, std::size_t MinTableSize>
class hash_table
{
public:
    using value_type = typename Traits::value_type;
    static_assert(std::is_trivial_v<value_type>);

    constexpr hash_table() : _table(nullptr), _table_capacity(0), _table_size(0) {}

    template <typename ResourcePtr>
    void free(ResourcePtr resource)
    {
        if (_table_capacity == 0)
            return;

        resource->deallocate(_table, _table_capacity * sizeof(value_type), alignof(value_type));
        _table          = nullptr;
        _table_size     = 0;
        _table_capacity = 0;
    }

    struct entry_handle
    {
        hash_table* _self;
        value_type* _entry;
        bool        _valid;

        explicit operator bool() const
        {
            return _valid;
        }

        std::size_t index() const
        {
            return std::size_t(_entry - _self->_table);
        }

        value_type& get() const
        {
            DRYAD_PRECONDITION(*this);
            return *_entry;
        }

        void create(const value_type& value)
        {
            DRYAD_PRECONDITION(!*this);
            *_entry = value;
            ++_self->_table_size;
            _valid = true;
        }

        void remove()
        {
            DRYAD_PRECONDITION(*this);
            Traits::fill_removed(_entry, 1);
            --_self->_table_size;
            _valid = false;
        }
    };

    // Looks for an entry in the table, creating one if necessary.
    //
    // If it is already in the table, returns a pointer to its valid entry.
    //
    // Otherwise, locates a new entry for that value and returns a pointer to it which is currently
    // invalid. Invariants of map are broken until the ptr has been written to.
    template <typename Key>
    entry_handle lookup_entry(const Key& key, Traits traits = {})
    {
        DRYAD_PRECONDITION(_table_size < _table_capacity);

        auto hash      = traits.hash(key);
        auto table_idx = hash & (_table_capacity - 1);

        while (true)
        {
            auto entry = _table + table_idx;
            if (Traits::is_unoccupied(*entry))
                // We found an empty entry, return it.
                return {this, entry, false};

            // Check whether the entry is the same string.
            if (traits.is_equal(*entry, key))
                // It is already in the table, return it.
                return {this, entry, true};

            // Go to next entry.
            table_idx = (table_idx + 1) & (_table_capacity - 1);
        }
    }
    template <typename Key>
    value_type* lookup(const Key& key, Traits traits = {}) const
    {
        if (_table_size == 0)
            return nullptr;

        auto entry = const_cast<hash_table*>(this)->lookup_entry(key, traits);
        return entry ? &entry.get() : nullptr;
    }

    bool should_rehash() const
    {
        return _table_size >= _table_capacity / 2;
    }

    static constexpr std::size_t to_table_capacity(unsigned long long cap)
    {
        if (cap < MinTableSize)
            return MinTableSize;

        // Round up to next power of two.
        return std::size_t(1) << (int(sizeof(cap) * CHAR_BIT) - __builtin_clzll(cap - 1));
    }

    template <typename ResourcePtr, typename Callback = void (*)(entry_handle, std::size_t)>
    void rehash(
        ResourcePtr resource, std::size_t new_capacity, Traits traits = {},
        Callback entry_cb = +[](entry_handle, std::size_t) {})
    {
        DRYAD_PRECONDITION(new_capacity == to_table_capacity(new_capacity));
        if (new_capacity <= _table_capacity)
            return;

        auto old_table    = _table;
        auto old_capacity = _table_capacity;

        // Allocate a bigger, currently empty table.
        _table = static_cast<value_type*>(
            resource->allocate(new_capacity * sizeof(value_type), alignof(value_type)));
        _table_capacity = new_capacity;
        Traits::fill_unoccupied(_table, _table_capacity);

        // Insert existing values into the new table.
        if (_table_size > 0)
        {
            _table_size = 0;

            for (auto entry = old_table; entry != old_table + old_capacity; ++entry)
                if (!Traits::is_unoccupied(*entry))
                {
                    auto new_entry = lookup_entry(*entry, traits);
                    new_entry.create(*entry);
                    entry_cb(new_entry, std::size_t(entry - old_table));
                }
        }
    }
    template <typename ResourcePtr, typename Callback = void (*)(entry_handle, std::size_t)>
    void rehash(
        ResourcePtr resource, Traits traits = {},
        Callback entry_cb = +[](entry_handle, std::size_t) {})
    {
        rehash(resource, to_table_capacity(2 * _table_capacity), traits, entry_cb);
    }

    //=== access ===//
    std::size_t size() const
    {
        return _table_size;
    }
    std::size_t capacity() const
    {
        return _table_capacity;
    }

    struct entry_range
    {
        struct iterator : _detail::forward_iterator_base<iterator, entry_handle, entry_handle, void>
        {
            hash_table* _self;
            value_type* _cur;

            iterator() : _self(nullptr), _cur(nullptr) {}
            explicit iterator(hash_table& self, value_type* cur) : _self(&self), _cur(cur) {}

            entry_handle deref() const
            {
                return {_self, _cur, true};
            }
            void increment()
            {
                auto end = _self->_table + _self->_table_capacity;
                do
                {
                    ++_cur;
                } while (_cur != end && Traits::is_unoccupied(*_cur));
            }
            bool equal(iterator rhs) const
            {
                return _cur == rhs._cur;
            }
        };

        iterator begin() const
        {
            if (_self->size() == 0)
                return {};

            auto cur = _self->_table;
            while (Traits::is_unoccupied(*cur))
                cur++;
            return iterator(*_self, cur);
        }
        iterator end() const
        {
            if (_self->size() == 0)
                return {};

            return iterator(*_self, _self->_table + _self->_table_capacity);
        }

        hash_table* _self;
    };

    /// Iterates over all occupied entries.
    entry_range entries()
    {
        return {this};
    }

private:
    value_type* _table;
    std::size_t _table_capacity; // power of two
    std::size_t _table_size;
};
} // namespace dryad::_detail

#endif // DRYAD_DETAIL_HASH_TABLE_HPP_INCLUDED

