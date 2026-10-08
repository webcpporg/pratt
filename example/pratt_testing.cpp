// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A test of a grammar, with lightweight_test: the openui-lang micro-subset's trees, its errors and
// its depth limit. It prints nothing on its standard output, and returns boost::report_errors().

// tag::includes[]
#include <webcpp/pratt.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>
#include <string_view>

#include "openui/grammar.hpp"

namespace pratt = webcpp::pratt;
namespace openui = webcpp::example::openui;
// end::includes[]

namespace {

// tag::helper[]
/** The parse of `line` by the micro-subset's grammar, as deep as `max_depth` allows. */
pratt::result<std::string> parsed(std::string_view line, int max_depth = pratt::default_max_depth) {
    return pratt::parse(openui::grammar{}, openui::stream(line), max_depth);
}

// end::helper[]

// tag::trees[]
void assignment_is_right_associative_and_a_call_binds_tightest() {
    BOOST_TEST_EQ(parsed("a = b = 1").value(), "(= a (= b 1))");
    BOOST_TEST_EQ(parsed("Stack(1, 2)(3)").value(), "(call (call Stack 1 2) 3)");
    BOOST_TEST_EQ(parsed("a = Text(gap: 4)").value(), "(= a (call Text (: gap 4)))");
}

// end::trees[]

// tag::errors[]
void every_error_is_a_value() {
    BOOST_TEST(parsed("Stack(1").error() == pratt::error::expected_closing_token);
    BOOST_TEST(parsed("= 3").error() == pratt::error::unexpected_token);
    BOOST_TEST(parsed("root @ 3").error() == pratt::error::invalid_character);
    BOOST_TEST(parsed("a b").error() == pratt::error::trailing_input);
}

void the_depth_is_bounded() {
    BOOST_TEST(parsed("((x))", 3).has_value());
    BOOST_TEST(parsed("(((x)))", 3).error() == pratt::error::nesting_too_deep);
}

// end::errors[]

}  // namespace

// tag::main[]
int main() {
    assignment_is_right_associative_and_a_call_binds_tightest();
    every_error_is_a_value();
    the_depth_is_bounded();
    return boost::report_errors();
}

// end::main[]
