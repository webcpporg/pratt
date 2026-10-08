// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests the openui-lang micro-subset the REPL example parses, through the grammar it shares with
// that example: every line of the reference session, the shapes `(` takes as a grouping and as a
// call, the right-associative `=`, and the errors of its lazy lexer.

#include <webcpp/pratt/parser.hpp>

#include <boost/core/lightweight_test.hpp>

#include <openui/grammar.hpp>

#include <string>
#include <string_view>

#include "require.hpp"

namespace pratt = webcpp::pratt;
namespace openui = webcpp::example::openui;

using webcpp::test::require;

static_assert(pratt::token_stream<openui::stream>);
static_assert(pratt::grammar_for<openui::grammar, openui::stream>);

namespace {

void parses_to(std::string_view source, std::string_view expected) {
    const pratt::result<std::string> parsed =
        pratt::parse(openui::grammar{}, openui::stream(source));
    require(BOOST_TEST(parsed.has_value()));
    BOOST_TEST_EQ(*parsed, expected);
}

void fails_with(std::string_view source, pratt::error expected) {
    const pratt::result<std::string> parsed =
        pratt::parse(openui::grammar{}, openui::stream(source));
    require(BOOST_TEST(parsed.has_error()));
    BOOST_TEST(parsed.error() == expected);
}

void the_reference_session_parses_as_recorded() {
    parses_to("root = Stack()", "(= root (call Stack))");
    parses_to(
        R"(root = Stack(spacing: 8, Button(label: "Pay"), Text("Total")))",
        R"((= root (call Stack (: spacing 8) (call Button (: label "Pay")) (call Text "Total"))))");
    parses_to(R"(view = Row(Button(label: "ok"), gap: 4))",
              R"((= view (call Row (call Button (: label "ok")) (: gap 4))))");
    parses_to("Stack(1, 2)(3)", "(call (call Stack 1 2) 3)");
    parses_to("(x)", "x");
}

void the_reference_session_fails_as_recorded() {
    fails_with("Stack(1", pratt::error::expected_closing_token);
    fails_with("= 3", pratt::error::unexpected_token);
    fails_with(R"(root = Stack(label: "unterminated))", pratt::error::unexpected_end);
    fails_with("root @ 3", pratt::error::invalid_character);
}

void a_call_without_arguments_is_the_callee_alone() {
    parses_to("Stack()", "(call Stack)");
}

void a_string_keeps_its_commas_and_colons() {
    parses_to(R"(Text("a, b: c"))", R"((call Text "a, b: c"))");
}

/** A string's `"..."` is found by its closing quote alone, so a `(` inside it never opens a call.
 */
void a_string_keeps_a_parenthesis_whole() {
    parses_to(R"(Text("a(b"))", R"((call Text "a(b"))");
}

void a_parenthesis_after_a_grouping_calls_it() {
    parses_to("(a)(b)", "(call a b)");
}

void assignment_associates_to_the_right() {
    parses_to("a = b = c", "(= a (= b c))");
}

void a_pair_binds_tighter_than_an_assignment() {
    parses_to("a = b: c", "(= a (: b c))");
    parses_to("a: b = c", "(= (: a b) c)");
}

void an_invalid_character_is_reported_where_it_stands() {
    fails_with("@", pratt::error::invalid_character);
    fails_with("Stack(1 @", pratt::error::invalid_character);
    fails_with("Stack(@", pratt::error::invalid_character);
    fails_with("a = @", pratt::error::invalid_character);
}

void an_unterminated_string_is_an_unexpected_end() {
    fails_with(R"(")", pratt::error::unexpected_end);
    fails_with(R"(Text "open)", pratt::error::unexpected_end);
    fails_with(R"(Text("open, 1))", pratt::error::unexpected_end);
}

void a_carriage_return_and_a_tab_separate_tokens() {
    parses_to("Stack(a,\tb)\r", "(call Stack a b)");
}

void every_other_misplaced_token_is_reported() {
    fails_with("", pratt::error::unexpected_end);
    fails_with("Stack(1,", pratt::error::unexpected_end);
    fails_with("Stack(1 2)", pratt::error::expected_closing_token);
    fails_with("(a", pratt::error::expected_closing_token);
    fails_with("a)", pratt::error::trailing_input);
    fails_with("a b", pratt::error::trailing_input);
    fails_with("Stack(,)", pratt::error::unexpected_token);
    fails_with("a: ", pratt::error::unexpected_end);
}

}  // namespace

int main() {
    the_reference_session_parses_as_recorded();
    the_reference_session_fails_as_recorded();
    a_call_without_arguments_is_the_callee_alone();
    a_string_keeps_its_commas_and_colons();
    a_string_keeps_a_parenthesis_whole();
    a_parenthesis_after_a_grouping_calls_it();
    assignment_associates_to_the_right();
    a_pair_binds_tighter_than_an_assignment();
    an_invalid_character_is_reported_where_it_stands();
    an_unterminated_string_is_an_unexpected_end();
    a_carriage_return_and_a_tab_separate_tokens();
    every_other_misplaced_token_is_reported();
    return boost::report_errors();
}
