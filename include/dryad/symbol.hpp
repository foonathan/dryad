// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_SYMBOL_HPP_INCLUDED
#define DRYAD_SYMBOL_HPP_INCLUDED

#include <cstring>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/hash_table.hpp>

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

template <typename IndexType, typename CharT>
struct symbol_index_hash_traits
{
    const symbol_buffer<CharT>* buffer;

    using value_type = IndexType;

    static constexpr bool is_unoccupied(IndexType index)
    {
        return index == IndexType(-1);
    }
    static void fill_unoccupied(IndexType* data, std::size_t size)
    {
        // It has all bits set to 1, so we can do it per-byte.
        std::memset(data, static_cast<unsigned char>(-1), size * sizeof(IndexType));
    }

    static constexpr bool is_equal(IndexType entry, IndexType value)
    {
        return entry == value;
    }
    bool is_equal(IndexType entry, const CharT* str) const
    {
        auto existing_str = buffer->c_str(entry);
        return std::strcmp(str, existing_str) == 0;
    }

    std::size_t hash(IndexType entry) const
    {
        return hash(buffer->c_str(entry));
    }
    static constexpr std::size_t hash(const CharT* str)
    {
        // FNV-1a 64 bit hash
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
    using traits       = _detail::symbol_index_hash_traits<IndexType, CharT>;

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
        _map.rehash(_resource, _map.to_table_capacity(number_of_symbols), traits{&_buffer});
    }

    symbol intern(const CharT* str, std::size_t length)
    {
        if (_map.should_rehash())
            _map.rehash(_resource, traits{&_buffer});

        auto entry = _map.lookup_entry(str, traits{&_buffer});
        if (entry)
            // Already interned, return index.
            return symbol(entry.get());

        // Copy string data to buffer, as we don't have it yet.
        _buffer.reserve_new_string(_resource, length);
        auto idx = _buffer.insert(str, length);
        DRYAD_PRECONDITION(idx == IndexType(idx)); // Overflow of index type.

        // Store index in map.
        entry.create(IndexType(idx));

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
    _detail::symbol_buffer<CharT>     _buffer;
    _detail::hash_table<traits, 1024> _map;
    DRYAD_EMPTY_MEMBER resource_ptr   _resource;

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

