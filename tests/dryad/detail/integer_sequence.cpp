// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <dryad/_detail/integer_sequence.hpp>

#include <doctest/doctest.h>

TEST_CASE("_detail::integer_sequence")
{
    using empty = dryad::_detail::index_sequence_for<>;
    CHECK(std::is_same_v<empty, dryad::_detail::index_sequence<>>);

    using one = dryad::_detail::index_sequence_for<int>;
    CHECK(std::is_same_v<one, dryad::_detail::index_sequence<0>>);

    using two = dryad::_detail::index_sequence_for<int, int>;
    CHECK(std::is_same_v<two, dryad::_detail::index_sequence<0, 1>>);

    using three = dryad::_detail::index_sequence_for<int, int, int>;
    CHECK(std::is_same_v<three, dryad::_detail::index_sequence<0, 1, 2>>);
}

