// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_HASH_ALGORITHM_HPP_INCLUDED
#define DRYAD_HASH_ALGORITHM_HPP_INCLUDED

#include <cstdint>
#include <dryad/_detail/config.hpp>

namespace dryad
{
/// FNV-1a 64 bit hash.
class default_hash_algorithm
{
    static constexpr std::uint64_t fnv_basis = 14695981039346656037ull;
    static constexpr std::uint64_t fnv_prime = 1099511628211ull;

public:
    explicit default_hash_algorithm() : _hash(fnv_basis) {}

    default_hash_algorithm(default_hash_algorithm&&)            = default;
    default_hash_algorithm& operator=(default_hash_algorithm&&) = default;

    ~default_hash_algorithm() = default;

    default_hash_algorithm&& hash_bytes(const unsigned char* ptr, std::size_t size)
    {
        for (auto i = 0u; i != size; ++i)
        {
            _hash ^= ptr[i];
            _hash *= fnv_prime;
        }
        return DRYAD_MOV(*this);
    }

    template <typename T, typename = std::enable_if_t<std::is_scalar_v<T>>>
    default_hash_algorithm&& hash_scalar(T value)
    {
        static_assert(!std::is_floating_point_v<T>,
                      "you shouldn't use floats as keys for a hash table");
        hash_bytes(reinterpret_cast<unsigned char*>(&value), sizeof(T));
        return DRYAD_MOV(*this);
    }

    template <typename CharT>
    default_hash_algorithm&& hash_c_str(const CharT* str)
    {
        while (*str != '\0')
        {
            hash_scalar(*str);
            ++str;
        }
        return DRYAD_MOV(*this);
    }

    std::uint64_t finish() &&
    {
        return _hash;
    }

private:
    std::uint64_t _hash;
};
} // namespace dryad

#endif // DRYAD_HASH_ALGORITHM_HPP_INCLUDED

