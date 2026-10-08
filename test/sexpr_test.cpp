// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests the engine through a grammar whose tokens are characters and whose nodes are strings, so
// that nothing of the calculator takes part.

#include <webcpp/pratt/parser.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>
#include <string_view>

#include "require.hpp"
#include "sexpr.hpp"

namespace pratt = webcpp::pratt;
namespace sexpr = webcpp::test::sexpr;

using webcpp::test::require;

static_assert(pratt::token_stream<sexpr::stream>);
static_assert(pratt::grammar_for<sexpr::grammar, sexpr::stream>);

namespace {

void parses_to(std::string_view source, std::string_view expected) {
    const pratt::result<std::string> parsed = pratt::parse(sexpr::grammar{}, sexpr::stream(source));
    require(BOOST_TEST(parsed.has_value()));
    BOOST_TEST_EQ(*parsed, expected);
}

void multiplication_binds_tighter_than_addition() {
    parses_to("1+2*3", "(+ 1 (* 2 3))");
}

void power_associates_to_the_right() {
    parses_to("2^3^2", "(^ 2 (^ 3 2))");
}

void a_negated_group_is_one_operand() {
    parses_to("-(1+2)*3", "(* (neg (+ 1 2)) 3)");
}

void subtraction_associates_to_the_left() {
    parses_to("1-2-3", "(- (- 1 2) 3)");
}

}  // namespace

int main() {
    multiplication_binds_tighter_than_addition();
    power_associates_to_the_right();
    a_negated_group_is_one_operand();
    subtraction_associates_to_the_left();
    return boost::report_errors();
}
