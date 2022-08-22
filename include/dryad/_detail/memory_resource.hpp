// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_DETAIL_MEMORY_RESOURCE_HPP_INCLUDED
#define DRYAD_DETAIL_MEMORY_RESOURCE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <new>

#if 0
// Subset of the interface of std::pmr::memory_resource.
class MemoryResource
{
public:
    void* allocate(std::size_t bytes, std::size_t alignment);
    void deallocate(void* ptr, std::size_t bytes, std::size_t alignment);

    friend bool operator==(const MemoryResource& lhs, const MemoryResource& rhs);
};
#endif

namespace dryad::_detail
{
class default_memory_resource
{
public:
    static void* allocate(std::size_t bytes, std::size_t alignment)
    {
        if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            return ::operator new (bytes, std::align_val_t{alignment});
        else
            return ::operator new(bytes);
    }

    static void deallocate(void* ptr, std::size_t bytes, std::size_t alignment) noexcept
    {
#ifdef __cpp_sized_deallocation
        if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            ::operator delete (ptr, bytes, std::align_val_t{alignment});
        else
            ::operator delete(ptr, bytes);
#else
        (void)bytes;

        if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            ::operator delete (ptr, std::align_val_t{alignment});
        else
            ::operator delete(ptr);
#endif
    }

    friend constexpr bool operator==(default_memory_resource, default_memory_resource) noexcept
    {
        return true;
    }
};
} // namespace dryad::_detail

namespace dryad::_detail
{
template <typename MemoryResource>
class _memory_resource_ptr_empty
{
public:
    constexpr explicit _memory_resource_ptr_empty(MemoryResource*) noexcept {}
    constexpr explicit _memory_resource_ptr_empty(void*) noexcept {}

    constexpr auto operator*() const noexcept
    {
        return MemoryResource{};
    }

    constexpr auto operator->() const noexcept
    {
        struct proxy
        {
            MemoryResource _resource;

            constexpr MemoryResource* operator->() noexcept
            {
                return &_resource;
            }
        };

        return proxy{};
    }

    constexpr MemoryResource* get() const noexcept
    {
        return nullptr;
    }
};

template <typename MemoryResource>
class _memory_resource_ptr
{
public:
    constexpr explicit _memory_resource_ptr(MemoryResource* resource) noexcept : _resource(resource)
    {
        LEXY_PRECONDITION(resource);
    }

    constexpr MemoryResource& operator*() const noexcept
    {
        return *_resource;
    }

    constexpr MemoryResource* operator->() const noexcept
    {
        return _resource;
    }

    constexpr MemoryResource* get() const noexcept
    {
        return _resource;
    }

private:
    MemoryResource* _resource;
};

// clang-format off
template <typename MemoryResource>
using memory_resource_ptr
    = std::conditional_t<std::is_void_v<MemoryResource>,
            _memory_resource_ptr_empty<default_memory_resource>,
            std::conditional_t<std::is_empty_v<MemoryResource>,
                _memory_resource_ptr_empty<MemoryResource>,
                _memory_resource_ptr<MemoryResource>>>;
// clang-format on

template <typename MemoryResource,
          typename
          = std::enable_if_t<std::is_void_v<MemoryResource> || std::is_empty_v<MemoryResource>>>
constexpr MemoryResource* get_memory_resource()
{
    return nullptr;
}
} // namespace dryad::_detail

#endif // DRYAD_DETAIL_MEMORY_RESOURCE_HPP_INCLUDED

