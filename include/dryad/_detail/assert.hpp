// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef DRYAD_DETAIL_ASSERT_HPP_INCLUDED
#define DRYAD_DETAIL_ASSERT_HPP_INCLUDED

#include <dryad/_detail/config.hpp>

#ifndef DRYAD_ENABLE_ASSERT

// By default, enable assertions if NDEBUG is not defined.

#    if NDEBUG
#        define DRYAD_ENABLE_ASSERT 0
#    else
#        define DRYAD_ENABLE_ASSERT 1
#    endif

#endif

#if DRYAD_ENABLE_ASSERT

// We want assertions: use assert() if that's available, otherwise abort.
// We don't use assert() directly as that's not constexpr.

#    if NDEBUG

#        include <cstdlib>
#        define DRYAD_PRECONDITION(Expr) ((Expr) ? void(0) : std::abort())
#        define DRYAD_ASSERT(Expr, Msg) ((Expr) ? void(0) : std::abort())

#    else

#        include <cassert>

#        define DRYAD_PRECONDITION(Expr) ((Expr) ? void(0) : assert(Expr))
#        define DRYAD_ASSERT(Expr, Msg) ((Expr) ? void(0) : assert((Expr) && (Msg)))

#    endif

#else

// We don't want assertions.

#    define DRYAD_PRECONDITION(Expr) static_cast<void>(sizeof(Expr))
#    define DRYAD_ASSERT(Expr, Msg) static_cast<void>(sizeof(Expr))

#endif

#endif // DRYAD_DETAIL_ASSERT_HPP_INCLUDED

