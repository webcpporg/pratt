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

/** `source` evaluates within `max_depth` to a value. */
template <class Number>
void evaluates(std::string_view source, int max_depth = pratt::default_max_depth) {
    fixed_environment<Number> env;
    const pratt::result<Number> value = calc::evaluate(source, env, max_depth);
    BOOST_TEST(value.has_value());
}

/** `source` is refused as too deep within `max_depth`. */
template <class Number>
void too_deep(std::string_view source, int max_depth = pratt::default_max_depth) {
    fixed_environment<Number> env;
    const pratt::result<Number> value = calc::evaluate(source, env, max_depth);
    BOOST_TEST(value.has_error() && value.error() == pratt::error::nesting_too_deep);
}

/** A grouping, a prefix chain and a right-associative chain at exactly the default depth. */
template <class Number>
void the_default_depth_is_evaluated() {
    constexpr int levels = pratt::default_max_depth - 1;
    evaluates<Number>(grouping(levels));
    evaluates<Number>(prefix_chain(levels));
    evaluates<Number>(right_chain(levels));
}

/** One level more than the default is refused, in each of the three shapes. */
template <class Number>
void one_level_more_is_too_deep() {
    constexpr int levels = pratt::default_max_depth;
    too_deep<Number>(grouping(levels));
    too_deep<Number>(prefix_chain(levels));
    too_deep<Number>(right_chain(levels));
}

/**
 The deepest parse the default allows uses at most half the stack: twice the default depth still
 evaluates when the limit is raised to it.

 Tip: on wasip2 and wasip3 in a debug build this is the measurement the default rests on; when it
 fails there, the default is to be measured again.
*/
template <class Number>
void twice_the_default_depth_fits_the_stack() {
    constexpr int twice = 2 * pratt::default_max_depth;
    evaluates<Number>(grouping(twice - 1), twice);
    evaluates<Number>(prefix_chain(twice - 1), twice);
    evaluates<Number>(right_chain(twice - 1), twice);
}

/** Ten thousand `(`, a line a user may type, is refused before the stack overflows. */
template <class Number>
void ten_thousand_parentheses_are_too_deep() {
    too_deep<Number>(std::string(10'000, '('));
}

template <class Number>
void every_case() {
    the_default_depth_is_evaluated<Number>();
    one_level_more_is_too_deep<Number>();
    twice_the_default_depth_fits_the_stack<Number>();
    ten_thousand_parentheses_are_too_deep<Number>();
}

}  // namespace

int main() {
    every_case<decimal64_t>();
    every_case<double>();
    return boost::report_errors();
}
