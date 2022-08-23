// Copyright (C) 2020-2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_DETAIL_TUPLE_HPP_INCLUDED
#define DRYAD_DETAIL_TUPLE_HPP_INCLUDED

#include <dryad/_detail/config.hpp>
#include <dryad/_detail/integer_sequence.hpp>

namespace dryad::_detail
{
template <std::size_t Idx, typename T>
struct _tuple_holder
{
#if !defined(__GNUC__) || defined(__clang__)
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105795
    DRYAD_EMPTY_MEMBER
#endif
    T value;
};

template <std::size_t Idx, typename... T>
struct _nth_type;
template <std::size_t Idx, typename H, typename... T>
struct _nth_type<Idx, H, T...>
{
    using type = typename _nth_type<Idx - 1, T...>::type;
};
template <typename H, typename... T>
struct _nth_type<0, H, T...>
{
    using type = H;
};

template <typename T>
struct _tuple_get_type
{
    using type = T&;
};
template <typename T>
struct _tuple_get_type<T&&>
{
    using type = T&&;
};

template <typename Indices, typename... T>
class _tuple;
template <std::size_t... Idx, typename... T>
class _tuple<index_sequence<Idx...>, T...> : public _tuple_holder<Idx, T>...
{
public:
    constexpr _tuple() = default;

    template <typename... Args>
    constexpr _tuple(Args&&... args) : _tuple_holder<Idx, T>{DRYAD_FWD(args)}...
    {}
};

template <typename... T>
struct tuple : _tuple<index_sequence_for<T...>, T...>
{
    constexpr tuple() = default;

    template <typename... Args>
    constexpr explicit tuple(Args&&... args)
    : _tuple<index_sequence_for<T...>, T...>(DRYAD_FWD(args)...)
    {}

    template <std::size_t N>
    using element_type = typename _nth_type<N, T...>::type;

    template <std::size_t N>
    constexpr decltype(auto) get() noexcept
    {
        // NOLINTNEXTLINE: this is fine.
        auto&& holder = static_cast<_tuple_holder<N, element_type<N>>&>(*this);
        // NOLINTNEXTLINE
        return static_cast<typename _tuple_get_type<element_type<N>>::type>(holder.value);
    }
    template <std::size_t N>
    constexpr decltype(auto) get() const noexcept
    {
        // NOLINTNEXTLINE: this is fine.
        auto&& holder = static_cast<const _tuple_holder<N, element_type<N>>&>(*this);
        // NOLINTNEXTLINE
        return static_cast<typename _tuple_get_type<const element_type<N>>::type>(holder.value);
    }

    static constexpr auto index_sequence()
    {
        return index_sequence_for<T...>{};
    }
};
template <>
struct tuple<>
{
    constexpr tuple() = default;

    static constexpr auto index_sequence()
    {
        return index_sequence_for<>{};
    }
};

template <typename... Args>
constexpr auto make_tuple(Args&&... args)
{
    return tuple<std::decay_t<Args>...>(DRYAD_FWD(args)...);
}

template <typename... Args>
constexpr auto forward_as_tuple(Args&&... args)
{
    return tuple<Args&&...>(DRYAD_FWD(args)...);
}
} // namespace dryad::_detail

#endif // DRYAD_DETAIL_TUPLE_HPP_INCLUDED

