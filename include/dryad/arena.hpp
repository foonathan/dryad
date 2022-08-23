// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_ARENA_HPP_INCLUDED
#define DRYAD_ARENA_HPP_INCLUDED

#include <dryad/_detail/assert.hpp>
#include <dryad/_detail/config.hpp>
#include <dryad/_detail/memory_resource.hpp>

namespace dryad::_detail
{
constexpr bool is_valid_alignment(std::size_t alignment) noexcept
{
    return alignment != 0u && (alignment & (alignment - 1)) == 0u;
}

constexpr std::size_t align_offset(std::uintptr_t address, std::size_t alignment) noexcept
{
    DRYAD_PRECONDITION(is_valid_alignment(alignment));
    auto misaligned = address & (alignment - 1);
    return misaligned != 0 ? (alignment - misaligned) : 0;
}
inline std::size_t align_offset(const void* address, std::size_t alignment) noexcept
{
    DRYAD_PRECONDITION(is_valid_alignment(alignment));
    return align_offset(reinterpret_cast<std::uintptr_t>(address), alignment);
}
} // namespace dryad::_detail

namespace dryad
{
/// A simple stack allocator.
template <typename MemoryResource = void>
class arena
{
    using resource_ptr = _detail::memory_resource_ptr<MemoryResource>;

    static constexpr auto block_size = std::size_t(16) * 1024 - sizeof(void*);

    struct block
    {
        block*        next;
        unsigned char memory[block_size];

        static block* allocate(resource_ptr resource)
        {
            auto memory = resource->allocate(sizeof(block), alignof(block));
            auto ptr    = ::new (memory) block; // Don't initialize array!
            ptr->next   = nullptr;
            return ptr;
        }

        static block* deallocate(resource_ptr resource, block* ptr)
        {
            auto next = ptr->next;
            resource->deallocate(ptr, sizeof(block), alignof(block));
            return next;
        }

        unsigned char* end() noexcept
        {
            return &memory[block_size];
        }
    };

public:
    //=== constructors/destructors/assignment ===//
    constexpr arena() noexcept : arena(_detail::get_memory_resource<MemoryResource>()) {}

    explicit constexpr arena(MemoryResource* resource) noexcept
    : _cur_block(nullptr), _cur_pos(nullptr), _resource(resource), _first_block(nullptr)
    {}

    arena(arena&& other) noexcept
    : _cur_block(other._cur_block), _cur_pos(other._cur_pos), _resource(other._resource),
      _first_block(other._first_block)
    {
        other._first_block = other._cur_block = nullptr;
        other._cur_pos                        = nullptr;
    }

    ~arena() noexcept
    {
        auto cur = _first_block;
        while (cur != nullptr)
            cur = block::deallocate(_resource, cur);
    }

    arena& operator=(arena&& other) noexcept
    {
        _detail::swap(_resource, other._resource);
        _detail::swap(_first_block, other._first_block);
        _detail::swap(_cur_block, other._cur_block);
        _detail::swap(_cur_pos, other._cur_pos);
        return *this;
    }

    //=== allocation ===//
    static constexpr auto max_allocation_size = block_size;

    void* allocate(std::size_t size, std::size_t alignment)
    {
        DRYAD_PRECONDITION(size <= max_allocation_size);
        DRYAD_PRECONDITION(alignment <= alignof(void*));

        auto offset    = _detail::align_offset(_cur_pos, alignment);
        auto remaining = _cur_block == nullptr ? 0 : std::size_t(_cur_block->end() - _cur_pos);
        if (offset + size > remaining)
        {
            if (_cur_block == nullptr)
            {
                DRYAD_ASSERT(_first_block == nullptr,
                             "_cur_block is always valid once we have a block");
                _first_block = block::allocate(_resource);
                _cur_block   = _first_block;
            }
            else
            {
                if (_cur_block->next == nullptr)
                    _cur_block->next = block::allocate(_resource);

                _cur_block = _cur_block->next;
            }

            _cur_pos = &_cur_block->memory[0];
            offset   = 0;
            DRYAD_ASSERT(_detail::align_offset(_cur_pos, alignment) == 0,
                         "beginning of block should be properly aligned");
        }

        _cur_pos += offset;
        auto result = _cur_pos;
        _cur_pos += size;
        return result;
    }

    template <typename T>
    void* allocate()
    {
        static_assert(sizeof(T) <= max_allocation_size);
        static_assert(alignof(T) <= alignof(void*));
        return allocate(sizeof(T), alignof(T));
    }

    template <typename T, typename... Args>
    T* construct(Args&&... args)
    {
        return ::new (allocate<T>()) T(DRYAD_FWD(args)...);
    }

    //=== unwinding ===//
    struct marker
    {
        block*         cur_block;
        unsigned char* cur_pos;
    };

    marker top() const
    {
        return {_cur_block, _cur_pos};
    }

    void unwind(marker m)
    {
        _cur_block = m.cur_block;
        _cur_pos   = m.cur_pos;
    }

    void clear()
    {
        if (_first_block == nullptr)
            // We never held data to begin with, so don't need to do anything.
            return;

        _cur_block = _first_block;
        _cur_pos   = &_cur_block->memory[0];
    }

private:
    block*         _cur_block;
    unsigned char* _cur_pos;

    DRYAD_EMPTY_MEMBER resource_ptr _resource;
    block*                          _first_block;
};
} // namespace dryad

#endif // DRYAD_ARENA_HPP_INCLUDED

