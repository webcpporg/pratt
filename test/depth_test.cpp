// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests the default depth limit with the calculator's own frames, over decimal64_t and over double:
// a grouping, a prefix chain and a right-associative chain exactly default_max_depth deep are
// evaluated and one level more is refused, twice the default still fits the stack, and a line of
// ten thousand `(` is refused instead of overflowing it. On wasip2 and wasip3 the stack is wasm's
// default 64 KiB, so these are the cases that measure the default.

#include <webcpp/pratt/calculator.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/decimal.hpp>

#include <iostream>
#include <string>
#include <string_view>

#include "environments.hpp"

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;

using boost::decimal::decimal64_t;
using webcpp::test::fixed_environment;

namespace {

/**
 `(` `levels` times, `1`, and `)` as many times: `levels + 1` expression calls deep.
*/
std::string grouping(int levels) {
    const auto count = static_cast<std::string::size_type>(levels);
    return std::string(count, '(') + "1" + std::string(count, ')');
}

/**
 `levels` unary minus signs before `1`, one level each: `levels + 1` expression calls deep.

 Tip: the signs are spaced, since `--` is a decrement.
*/
std::string prefix_chain(int levels) {
    std::string source;
    for (int i = 0; i < levels; ++i) {
        source += "- ";
    }
    return source + "1";
}

/**
 `2 ^ 2 ^ ... ^ 2` with `levels` operators, one level each: `levels + 1` expression calls deep.

 Tip: of the three shapes, this one costs the most stack per level, as `^` is evaluated in
 a frame of its own on the way back up.
*/
std::string right_chain(int levels) {
    std::string source;
    for (int i = 0; i < levels; ++i) {
        source += "2 ^ ";
    }
    return source + "2";
}

/**
 Reports the failure of the case `what` over `number`: what it expected, what it got, and what
 the failure means when the case says.
*/
void fail(std::string_view what, std::string_view number, std::string_view expected,
          std::string_view got, std::string_view meaning = {}) {
    std::string message(what);
    message += " (";
    message += number;
    message += "): expected ";
    message += expected;
    message += ", got ";
    message += got;
    if (!meaning.empty()) {
        message += ". ";
        message += meaning;
    }
    BOOST_ERROR(message.c_str());
}

/** The case `what`: `source` evaluates within `max_depth` to a value; `meaning` is a failure's. */
template <class Number>
void evaluates(std::string_view what, std::string_view number, std::string_view source,
               int max_depth = pratt::default_max_depth, std::string_view meaning = {}) {
    fixed_environment<Number> env;
    const pratt::result<Number> value = calc::evaluate(source, env, max_depth);
    if (value.has_error()) {
        fail(what, number, "a value", value.error().message(), meaning);
    }
}

/** The case `what`: `source` is refused as too deep within `max_depth`. */
template <class Number>
void too_deep(std::string_view what, std::string_view number, std::string_view source,
              int max_depth = pratt::default_max_depth) {
    fixed_environment<Number> env;
    const pratt::result<Number> value = calc::evaluate(source, env, max_depth);
    if (value.has_value()) {
        fail(what, number, "nesting_too_deep", "a value");
    } else if (value.error() != pratt::error::nesting_too_deep) {
        fail(what, number, "nesting_too_deep", value.error().message());
    }
}

/** A grouping, a prefix chain and a right-associative chain at exactly the default depth. */
template <class Number>
void the_default_depth_is_evaluated(std::string_view number) {
    constexpr int levels = pratt::default_max_depth - 1;
    evaluates<Number>("a grouping default_max_depth deep", number, grouping(levels));
    evaluates<Number>("a prefix chain default_max_depth deep", number, prefix_chain(levels));
    evaluates<Number>("a right-associative chain default_max_depth deep", number,
                      right_chain(levels));
}

/** One level more than the default is refused, in each of the three shapes. */
template <class Number>
void one_level_more_is_too_deep(std::string_view number) {
    constexpr int levels = pratt::default_max_depth;
    too_deep<Number>("a grouping one level deeper than default_max_depth", number,
                     grouping(levels));
    too_deep<Number>("a prefix chain one level deeper than default_max_depth", number,
                     prefix_chain(levels));
    too_deep<Number>("a right-associative chain one level deeper than default_max_depth", number,
                     right_chain(levels));
}

/** What a failure of the canary means, written before it runs and with each failure it reports. */
constexpr std::string_view canary_meaning =
    "default_max_depth no longer has its 2x stack margin on this toolchain and must be measured "
    "again (parser.hpp)";

/**
 The canary: the deepest parse the default allows uses at most half the stack, so twice the
 default depth still evaluates when the limit is raised to it.

 Tip: on wasip2 and wasip3 in a debug build this is the measurement the default rests on. There a
 failure is a stack overflow, a trap that ends the program before anything can be reported, so the
 canary writes what a trap means before it runs; the line is followed by a trap only when the
 margin is gone.
*/
template <class Number>
void twice_the_default_depth_fits_the_stack(std::string_view number) {
    constexpr int twice = 2 * pratt::default_max_depth;
    std::cerr << "the canary (" << number << "): if a trap follows, " << canary_meaning << '\n';
    evaluates<Number>("the canary, a grouping twice default_max_depth deep", number,
                      grouping(twice - 1), twice, canary_meaning);
    evaluates<Number>("the canary, a prefix chain twice default_max_depth deep", number,
                      prefix_chain(twice - 1), twice, canary_meaning);
    evaluates<Number>("the canary, a right-associative chain twice default_max_depth deep", number,
                      right_chain(twice - 1), twice, canary_meaning);
}

/** Ten thousand `(`, a line a user may type, is refused before the stack overflows. */
template <class Number>
void ten_thousand_parentheses_are_too_deep(std::string_view number) {
    too_deep<Number>("ten thousand (", number, std::string(10'000, '('));
}

template <class Number>
void every_case(std::string_view number) {
    the_default_depth_is_evaluated<Number>(number);
    one_level_more_is_too_deep<Number>(number);
    twice_the_default_depth_fits_the_stack<Number>(number);
    ten_thousand_parentheses_are_too_deep<Number>(number);
}

}  // namespace

int main() {
    every_case<decimal64_t>("decimal64_t");
    every_case<double>("double");
    return boost::report_errors();
}
