// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The engine: include this one header to use pratt's parser without its
 calculator. It leaves out `<webcpp/pratt/calculator.hpp>`, which brings
 Boost.Decimal, a heavy dependency the engine does not need; the
 calculator is included by name.

 @see "Introduction", in the guide.
*/
#ifndef WEBCPP_PRATT_HPP
#define WEBCPP_PRATT_HPP

#include <webcpp/pratt/config.hpp>

#include <webcpp/pratt/errors.hpp>
#include <webcpp/pratt/parser.hpp>

#endif  // WEBCPP_PRATT_HPP
