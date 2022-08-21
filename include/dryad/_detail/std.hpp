// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_DETAIL_STD_HPP_INCLUDED
#define DRYAD_DETAIL_STD_HPP_INCLUDED

#include <dryad/_detail/config.hpp>

#if defined(__GLIBCXX__)

namespace std
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION
struct forward_iterator_tag;
struct bidirectional_iterator_tag;
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std

#elif defined(_LIBCPP_VERSION)

_LIBCPP_BEGIN_NAMESPACE_STD
struct forward_iterator_tag;
struct bidirectional_iterator_tag;
_LIBCPP_END_NAMESPACE_STD

#else

// Forward declaring things in std is not allowed, but I'm willing to take the risk.

namespace std
{
struct forward_iterator_tag;
struct bidirectional_iterator_tag;
} // namespace std

#endif

#endif // DRYAD_DETAIL_STD_HPP_INCLUDED

