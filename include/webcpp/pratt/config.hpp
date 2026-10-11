// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The configuration of pratt, which each of its headers includes first: whether
 pratt is built without exceptions.

 @see "Testing", in the guide.
*/
#ifndef WEBCPP_PRATT_CONFIG_HPP
#define WEBCPP_PRATT_CONFIG_HPP

#include <webcpp/config.hpp>

// MrDocs parses pratt as a build with exceptions does, where the macro is not
// defined: this definition, undone at once, is the one the reference lists, and
// the parse goes on with every API that throws in it.
#ifdef __MRDOCS__
/**
 Defined when pratt is built without exceptions: when webcpp has none (the
 compiler has none, or the developer defined `WEBCPP_NO_EXCEPTIONS` for every
 webcpp library), or when the developer defines it alone, to disable pratt's
 in a build that has them.

 pratt raises no exception of its own: a parse that fails returns its error, so
 the macro changes nothing in it. It is provided so that a program can set it
 for every webcpp library alike. An exception pratt raised would go through
 `boost::throw_exception`, and an API that throws would be absent while this is
 defined.

 @see "Testing", in the guide.
*/
#define WEBCPP_PRATT_NO_EXCEPTIONS
#undef WEBCPP_PRATT_NO_EXCEPTIONS
#endif

#if !defined(WEBCPP_HAS_EXCEPTIONS) && !defined(WEBCPP_PRATT_NO_EXCEPTIONS)
#define WEBCPP_PRATT_NO_EXCEPTIONS
#endif

#endif  // WEBCPP_PRATT_CONFIG_HPP
