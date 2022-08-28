// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_SYMBOL_TABLE_HPP_INCLUDED
#define DRYAD_SYMBOL_TABLE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/_detail/hash_table.hpp>
#include <dryad/_detail/iterator.hpp>
#include <dryad/_detail/memory_resource.hpp>
#include <dryad/symbol.hpp>

namespace dryad::_detail
{
template <typename IndexType>
struct symbol_hash_traits
{
    using value_type = IndexType;

    static bool is_unoccupied(IndexType idx)
    {
        return idx >= IndexType(-2);
    }

    static void fill_removed(IndexType* data, std::size_t size)
    {
        for (auto i = 0u; i < size; ++i)
            data[i] = IndexType(-2);
    }

    static void fill_unoccupied(IndexType* data, std::size_t size)
    {
        std::memset(data, static_cast<unsigned char>(-1), size * sizeof(IndexType));
    }

    static bool is_equal(IndexType entry, IndexType value)
    {
        return entry == value;
    }

    static std::size_t hash(IndexType entry)
    {
        return static_cast<std::size_t>(entry);
    }
};
} // namespace dryad::_detail

namespace dryad
{
/// Maps symbols to their declarations.
/// API assumes DeclRef is some sort of (smart) pointer to a declaration.
template <typename Symbol, typename DeclRef, typename MemoryResource = void>
class symbol_table
{
    static_assert(std::is_trivial_v<DeclRef>);

    using resource_ptr = _detail::memory_resource_ptr<MemoryResource>;
    using traits       = _detail::symbol_hash_traits<typename Symbol::index_type>;
    using hash_table   = _detail::hash_table<traits, 64>;

public:
    using symbol   = Symbol;
    using decl_ref = DeclRef;

    struct value_type
    {
        Symbol  symbol;
        DeclRef decl;
    };

    //=== constructors ===//
    constexpr symbol_table()
    : _decls(nullptr), _resource(_detail::get_memory_resource<MemoryResource>())
    {}
    constexpr explicit symbol_table(MemoryResource* resource) : _decls(nullptr), _resource(resource)
    {}

    ~symbol_table()
    {
        _resource->deallocate(_decls, _table.capacity() * sizeof(DeclRef), alignof(DeclRef));
        _table.free(_resource);
    }

    symbol_table(symbol_table&& other) noexcept
    : _table(other._table), _decls(other._decls), _resource(other._resource)
    {
        other._table = {};
        other._decls = {};
    }

    symbol_table& operator=(symbol_table&& other) noexcept
    {
        _detail::swap(_table, other._table);
        _detail::swap(_decls, other._decls);
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

    struct iterator : _detail::forward_iterator_base<iterator, value_type, value_type, void>
    {
        const symbol_table*                        _self = nullptr;
        typename hash_table::entry_range::iterator _cur;

        value_type deref() const
        {
            return {symbol(_cur->get()), _self->_decls[_cur->index()]};
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

    iterator begin() const
    {
        return {{}, this, const_cast<symbol_table*>(this)->_table.entries().begin()};
    }
    iterator end() const
    {
        return {{}, this, const_cast<symbol_table*>(this)->_table.entries().end()};
    }

    //=== modifiers ===//
    void rehash(std::size_t new_capacity)
    {
        new_capacity = _table.to_table_capacity(new_capacity);
        if (new_capacity <= _table.capacity())
            return;

        auto new_decls = static_cast<DeclRef*>(
            _resource->allocate(new_capacity * sizeof(DeclRef), alignof(DeclRef)));

        _table.rehash(_resource, new_capacity, traits{}, [&](auto new_entry, auto old_idx) {
            // Copy the entries over.
            new_decls[new_entry.index()] = _decls[old_idx];
        });
        _decls = new_decls;
    }

    /// Inserts a new declaration into the symbol table.
    /// If there is already a declaration with that name, replaces it and returns it.
    /// Otherwise returns a default constructed DeclRef.
    DeclRef insert_or_shadow(Symbol symbol, DeclRef decl)
    {
        if (_table.should_rehash())
            rehash(2 * _table.capacity());

        auto entry = _table.lookup_entry(symbol.id());
        if (entry)
        {
            auto shadowed         = _decls[entry.index()];
            _decls[entry.index()] = decl;
            return shadowed;
        }
        else
        {
            entry.create(symbol.id());
            _decls[entry.index()] = decl;
            return DeclRef();
        }
    }

    /// Removes a declaration with that name from the symbol table.
    DeclRef remove(Symbol symbol)
    {
        if (empty())
            return DeclRef();

        auto entry = _table.lookup_entry(symbol.id());
        if (!entry)
            return DeclRef();

        auto decl = _decls[entry.index()];
        entry.remove();
        return decl;
    }

    /// Searches for a declaration with that name.
    /// If found in the symbol table, returns it.
    /// Otherwise, returns a default constructed DeclRef.
    DeclRef lookup(Symbol symbol) const
    {
        if (empty())
            return DeclRef();

        auto entry = const_cast<symbol_table*>(this)->_table.lookup_entry(symbol.id());
        return entry ? _decls[entry.index()] : DeclRef();
    }

private:
    hash_table                      _table;
    DeclRef*                        _decls; // shares key index
    DRYAD_EMPTY_MEMBER resource_ptr _resource;
};
} // namespace dryad

#endif // DRYAD_SYMBOL_TABLE_HPP_INCLUDED

