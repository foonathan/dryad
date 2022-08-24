// Copyright (C) 2022 Jonathan Müller and dryad contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>

#include <dryad/symbol.hpp>

int main()
{
    dryad::symbol_interner<0, char, unsigned> symbols;

    auto a1 = symbols.intern("a");
    auto a2 = symbols.intern("a");
    auto b  = symbols.intern("b");
    std::printf("%d %d %d\n", a1.id(), a2.id(), b.id());
}

