// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_SYMBOL_HPP_INCLUDED
#define DRYAD_SYMBOL_HPP_INCLUDED

#include <climits>
#include <cstring>
#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/memory_resource.hpp>

#include <cstdio>

namespace dryad::_detail
{
// Contains all unique symbols, null-terminated, in memory one after the other.
template <typename CharT>
class symbol_buffer
{
    static constexpr auto min_buffer_size = 16 * 1024;

public:
    constexpr symbol_buffer() : _data(nullptr), _size(0), _capacity(0) {}

    template <typename ResourcePtr>
    void free(ResourcePtr resource)
    {
        if (_capacity == 0)
            return;

        resource->deallocate(_data, _capacity * sizeof(CharT), alignof(CharT));
        _data     = nullptr;
        _size     = 0;
        _capacity = 0;
    }

    template <typename ResourcePtr>
    void reserve(ResourcePtr resource, std::size_t new_capacity)
    {
        if (new_capacity <= _capacity)
            return;

        auto new_data
            = static_cast<CharT*>(resource->allocate(new_capacity * sizeof(CharT), alignof(CharT)));
        if (_size > 0)
            std::memcpy(new_data, _data, _size * sizeof(CharT));

        _data     = new_data;
        _capacity = new_capacity;
    }

    template <typename ResourcePtr>
    void reserve_new_string(ResourcePtr resource, std::size_t new_string_length)
    {
        // +1 for null-terminator.
        auto new_size = _size + new_string_length + 1;
        if (new_size <= _capacity)
            return;

        auto new_capacity = new_size * 2;
        if (new_capacity < min_buffer_size)
            new_capacity = min_buffer_size;

        reserve(resource, new_capacity);
    }

    std::size_t insert(const CharT* str, std::size_t length)
    {
        DRYAD_PRECONDITION(_capacity - _size >= length + 1);

        auto index = _size;

        std::memcpy(_data + _size, str, length * sizeof(CharT));
        _size += length;

        _data[_size] = CharT(0);
        ++_size;

        return index;
    }

    const CharT* c_str(std::size_t index) const
    {
        DRYAD_PRECONDITION(index < _size);
        return _data + index;
    }

private:
    CharT*      _data;
    std::size_t _size;
    std::size_t _capacity;
};

// Maps a string to its index.
// TODO: investigate quadratic probing, different max load factor.
template <typename CharT, typename IndexType>
class unique_symbol_map
{
    static constexpr auto min_table_size = 1024u;

public:
    static constexpr auto invalid_index = IndexType(-1);

    constexpr unique_symbol_map() : _table(nullptr), _table_capacity(0), _table_size(0) {}

    template <typename ResourcePtr>
    void free(ResourcePtr resource)
    {
        if (_table_capacity == 0)
            return;

        resource->deallocate(_table, _table_capacity * sizeof(IndexType), alignof(IndexType));
        _table          = nullptr;
        _table_size     = 0;
        _table_capacity = 0;
    }

    template <typename ResourcePtr>
    void rehash(ResourcePtr resource, const symbol_buffer<CharT>& symbols, std::size_t new_capacity)
    {
        new_capacity = to_table_capacity(new_capacity);
        if (new_capacity <= _table_capacity)
            return;

        auto old_table    = _table;
        auto old_capacity = _table_capacity;

        _table = static_cast<IndexType*>(
            resource->allocate(new_capacity * sizeof(IndexType), alignof(IndexType)));
        _table_capacity = new_capacity;

        // Set the new table to invalid_index.
        // It has all bits set to 1, so we can do it per-byte.
        std::memset(_table, static_cast<unsigned char>(invalid_index),
                    _table_capacity * sizeof(IndexType));

        // Insert existing strings into the new table.
        if (_table_size > 0)
        {
            _table_size = 0;

            for (auto entry = old_table; entry != old_table + old_capacity; ++entry)
                if (*entry != invalid_index)
                {
                    auto new_entry = lookup_entry(symbols, symbols.c_str(*entry));
                    DRYAD_ASSERT(*new_entry == invalid_index, "we don't have duplicates");
                    // The index didn't change, so copy over.
                    *new_entry = *entry;
                }
        }
    }
    template <typename ResourcePtr>
    void rehash(ResourcePtr resource, const symbol_buffer<CharT>& symbols)
    {
        rehash(resource, symbols, 2 * _table_capacity);
    }

    // Looks for the string in the table.
    //
    // If it is already in the table, returns a pointer to its index which is != invalid_index.
    //
    // Otherwise, locates a new entry for that string and returns a pointer to it which stores
    // invalid_index. Invariants of map are broken until the ptr has been written to.
    IndexType* lookup_entry(const symbol_buffer<CharT>& symbols, const CharT* str)
    {
        DRYAD_PRECONDITION(_table_size < _table_capacity);

        auto hash      = string_hash(str);
        auto table_idx = hash & (_table_capacity - 1);
        auto entry     = _table + table_idx;

        while (true)
        {
            if (*entry == invalid_index)
            {
                // We found an empty entry, return it.
                ++_table_size;
                return entry;
            }

            // Check whether the entry is the same string.
            if (auto existing_str = symbols.c_str(*entry); std::strcmp(str, existing_str) == 0)
            {
                // It is already in the table.
                // Return the occupied entry.
                return &_table[table_idx];
            }

            // Go to next entry.
            ++entry;
            if (entry == _table + _table_capacity)
                entry = _table;
        }
    }

    bool should_rehash() const
    {
        return _table_size >= _table_capacity / 2;
    }

private:
    static constexpr std::size_t to_table_capacity(unsigned long long cap)
    {
        if (cap < min_table_size)
            return min_table_size;

        // Round up to next power of two.
        return std::size_t(1) << (int(sizeof(cap) * CHAR_BIT) - __builtin_clzll(cap - 1));
    }

    // FNV-1a 64 bit hash
    static constexpr std::uint64_t string_hash(const CharT* str)
    {
        constexpr std::uint64_t fnv_basis = 14695981039346656037ull;
        constexpr std::uint64_t fnv_prime = 1099511628211ull;

        auto result = fnv_basis;
        for (; *str != CharT(0); ++str)
        {
            auto byte = static_cast<std::make_unsigned_t<CharT>>(*str);
            result ^= byte;
            result *= fnv_prime;
        }
        return result;
    }

    IndexType*  _table;
    std::size_t _table_capacity; // power of two
    std::size_t _table_size;
};
} // namespace dryad::_detail

namespace dryad
{
template <typename Id, typename IndexType = std::size_t>
class symbol;

template <typename Id, typename CharT = char, typename IndexType = std::size_t,
          typename MemoryResource = void>
class symbol_interner
{
    static_assert(std::is_trivial_v<CharT>);
    static_assert(std::is_unsigned_v<IndexType>);

    using resource_ptr = _detail::memory_resource_ptr<MemoryResource>;

public:
    using symbol = dryad::symbol<Id, IndexType>;

    //=== construction ===//
    constexpr symbol_interner() : _resource(_detail::get_memory_resource<MemoryResource>()) {}
    constexpr explicit symbol_interner(MemoryResource* resource) : _resource(resource) {}

    ~symbol_interner() noexcept
    {
        _buffer.free(_resource);
        _map.free(_resource);
    }

    symbol_interner(symbol_interner&& other) noexcept
    : _buffer(other._buffer), _map(other._map), _resource(other._resource)
    {
        other._buffer = {};
        other._map    = {};
    }

    symbol_interner& operator=(symbol_interner&& other) noexcept
    {
        _detail::swap(_buffer, other._buffer);
        _detail::swap(_map, other._map);
        _detail::swap(_resource, other._resource);
        return *this;
    }

    //=== interning ===//
    void reserve(std::size_t number_of_symbols, std::size_t average_symbol_length)
    {
        _buffer.reserve(_resource, number_of_symbols * average_symbol_length);
        _map.rehash(_resource, _buffer, number_of_symbols);
    }

    symbol intern(const CharT* str, std::size_t length)
    {
        if (_map.should_rehash())
            _map.rehash(_resource, _buffer);

        auto entry = _map.lookup_entry(_buffer, str);
        if (*entry != _map.invalid_index)
            // Already interned, return index.
            return symbol(*entry);

        // Copy string data to buffer, as we don't have it yet.
        _buffer.reserve_new_string(_resource, length);
        auto idx = _buffer.insert(str, length);
        DRYAD_PRECONDITION(idx == IndexType(idx)); // Overflow of index type.

        // Store index in map.
        *entry = IndexType(idx);

        // Return new symbol.
        return symbol(IndexType(idx));
    }
    template <std::size_t N>
    symbol intern(const CharT (&literal)[N])
    {
        DRYAD_PRECONDITION(literal[N - 1] == CharT(0));
        return intern(literal, N - 1);
    }

private:
    _detail::symbol_buffer<CharT>                _buffer;
    _detail::unique_symbol_map<CharT, IndexType> _map;
    DRYAD_EMPTY_MEMBER resource_ptr              _resource;

    friend symbol;
};
} // namespace dryad

namespace dryad
{
template <typename Id, typename IndexType>
class symbol
{
    static_assert(std::is_unsigned_v<IndexType>);

public:
    constexpr symbol() : _index(IndexType(-1)) {}

    //=== fast access ===//
    constexpr explicit operator bool() const
    {
        return _index != IndexType(-1);
    }

    constexpr IndexType id() const
    {
        return _index;
    }

    //=== slow access ===//
    template <typename CharT, typename MemoryResource>
    constexpr const CharT* c_str(
        const symbol_interner<Id, CharT, IndexType, MemoryResource>& interner) const
    {
        return interner._buffer.c_str(_index);
    }

    //=== comparison ===//
    friend constexpr bool operator==(symbol lhs, symbol rhs)
    {
        return lhs._index == rhs._index;
    }
    friend constexpr bool operator!=(symbol lhs, symbol rhs)
    {
        return lhs._index != rhs._index;
    }

    friend constexpr bool operator<(symbol lhs, symbol rhs)
    {
        return lhs._index < rhs._index;
    }
    friend constexpr bool operator<=(symbol lhs, symbol rhs)
    {
        return lhs._index <= rhs._index;
    }
    friend constexpr bool operator>(symbol lhs, symbol rhs)
    {
        return lhs._index > rhs._index;
    }
    friend constexpr bool operator>=(symbol lhs, symbol rhs)
    {
        return lhs._index >= rhs._index;
    }

private:
    constexpr explicit symbol(IndexType idx) : _index(idx) {}

    IndexType _index;

    template <typename, typename, typename, typename>
    friend class symbol_interner;
};
} // namespace dryad

#endif // DRYAD_SYMBOL_HPP_INCLUDED

