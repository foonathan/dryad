// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/_detail/std.hpp>

#include <iterator>

#include <doctest/doctest.h>

TEST_CASE("std forward declarations")
{
    // We just create them and see whether it's ambiguous.
    (void)std::forward_iterator_tag{};
    (void)std::bidirectional_iterator_tag{};
}

