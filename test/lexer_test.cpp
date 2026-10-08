// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests the calculator's number concept and its lazy lexer, over decimal64_t and over double: the
// kinds, texts and values of the tokens, numbers built in the number type's own arithmetic and
// past its range, the invalid token, whitespace, and the end token that stays.

#include <webcpp/pratt/calculator.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/decimal.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "require.hpp"

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;

using boost::decimal::decimal64_t;
using webcpp::test::require;

static_assert(calc::number<decimal64_t>);
static_assert(calc::number<double>);
static_assert(!calc::number<std::string>);
// std's pow, fmod and trunc of an int answer a double, so int is not closed under them.
static_assert(!calc::number<int>);
static_assert(pratt::token_stream<calc::stream<decimal64_t>>);
static_assert(pratt::token_stream<calc::stream<double>>);

namespace {

/** A token a source is expected to lex to. */
template <class Number>
struct expected_token {
    calc::kind type{};
    std::string_view text{};
    Number value{};
};

/** The name of a kind, so that a failure prints what was lexed. */
std::string_view name_of(calc::kind type) {
    switch (type) {
        case calc::kind::number: return "number";
        case calc::kind::name: return "name";
        case calc::kind::plus: return "plus";
        case calc::kind::minus: return "minus";
        case calc::kind::star: return "star";
        case calc::kind::slash: return "slash";
        case calc::kind::percent: return "percent";
        case calc::kind::caret: return "caret";
        case calc::kind::lparen: return "lparen";
        case calc::kind::rparen: return "rparen";
        case calc::kind::assign: return "assign";
        case calc::kind::eq: return "eq";
        case calc::kind::ne: return "ne";
        case calc::kind::lt: return "lt";
        case calc::kind::le: return "le";
        case calc::kind::gt: return "gt";
        case calc::kind::ge: return "ge";
        case calc::kind::increment: return "increment";
        case calc::kind::decrement: return "decrement";
        case calc::kind::invalid: return "invalid";
        case calc::kind::end: return "end";
    }
    return "unknown";
}

/**
 Lexes `source` and checks that it yields `tokens`, then the end token.

 Tip: each token is checked through peek, then through advance, which must return the same one.
*/
template <class Number>
void lexes_to(std::string_view source, const std::vector<expected_token<Number>>& tokens) {
    calc::stream<Number> lexer(source);
    for (const expected_token<Number>& want : tokens) {
        BOOST_TEST_EQ(name_of(lexer.peek().type), name_of(want.type));
        BOOST_TEST(!lexer.at_end());
        const calc::token<Number> got = lexer.advance();
        BOOST_TEST_EQ(name_of(got.type), name_of(want.type));
        BOOST_TEST_EQ(got.text, want.text);
        BOOST_TEST(got.value == want.value);
    }
    BOOST_TEST(lexer.at_end());
    const calc::token<Number> end = lexer.advance();
    BOOST_TEST_EQ(name_of(end.type), "end");
    BOOST_TEST(end.text.empty());
    BOOST_TEST(end.value == Number(0));
}

/** The value `source` lexes to, which must be one number token and nothing else. */
template <class Number>
Number number_of(std::string_view source) {
    calc::stream<Number> lexer(source);
    const calc::token<Number> token = lexer.advance();
    require(BOOST_TEST_EQ(name_of(token.type), "number"));
    BOOST_TEST_EQ(token.text, source);
    BOOST_TEST(lexer.at_end());
    return token.value;
}

template <class Number>
void an_assignment_lexes_with_its_texts_and_values() {
    using calc::kind;
    lexes_to<Number>("a = 43 + 12", {
                                        {kind::name, "a", Number(0)},
                                        {kind::assign, "=", Number(0)},
                                        {kind::number, "43", Number(43)},
                                        {kind::plus, "+", Number(0)},
                                        {kind::number, "12", Number(12)},
                                    });
}

template <class Number>
void every_two_character_operator_is_one_token() {
    using calc::kind;
    lexes_to<Number>("++ -- == != <= >=", {
                                              {kind::increment, "++", Number(0)},
                                              {kind::decrement, "--", Number(0)},
                                              {kind::eq, "==", Number(0)},
                                              {kind::ne, "!=", Number(0)},
                                              {kind::le, "<=", Number(0)},
                                              {kind::ge, ">=", Number(0)},
                                          });
    lexes_to<Number>("+++", {
                                {kind::increment, "++", Number(0)},
                                {kind::plus, "+", Number(0)},
                            });
}

template <class Number>
void every_one_character_operator_is_one_token() {
    using calc::kind;
    lexes_to<Number>("+-*/%^()=<>", {
                                        {kind::plus, "+", Number(0)},
                                        {kind::minus, "-", Number(0)},
                                        {kind::star, "*", Number(0)},
                                        {kind::slash, "/", Number(0)},
                                        {kind::percent, "%", Number(0)},
                                        {kind::caret, "^", Number(0)},
                                        {kind::lparen, "(", Number(0)},
                                        {kind::rparen, ")", Number(0)},
                                        {kind::assign, "=", Number(0)},
                                        {kind::lt, "<", Number(0)},
                                        {kind::gt, ">", Number(0)},
                                    });
}

template <class Number>
void a_name_is_letters_digits_and_underscores() {
    using calc::kind;
    lexes_to<Number>("_x1 Ab9 2y", {
                                       {kind::name, "_x1", Number(0)},
                                       {kind::name, "Ab9", Number(0)},
                                       {kind::number, "2", Number(2)},
                                       {kind::name, "y", Number(0)},
                                   });
}

void a_decimal_fraction_is_exact_in_decimal() {
    BOOST_TEST(number_of<decimal64_t>("1.5") == decimal64_t(15, -1));
    BOOST_TEST(number_of<decimal64_t>("0.1") == decimal64_t(1, -1));
    BOOST_TEST(number_of<decimal64_t>("12.034") == decimal64_t(12034, -3));
    BOOST_TEST(number_of<decimal64_t>("0.000000000000001") == decimal64_t(1, -15));
    // In decimal, 0.1 + 0.2 is 0.3, which no double sum of the two literals is.
    BOOST_TEST(number_of<decimal64_t>("0.1") + number_of<decimal64_t>("0.2") ==
               number_of<decimal64_t>("0.3"));
}

/**
 In double, a fraction lexes to the literal's value, the nearest double.

 Tip: 1.005 and 0.07 are among the values a division by ten per fraction digit misses by one
 unit in the last place; one division by ten to the number of digits finds them.
*/
void a_double_fraction_is_the_nearest_double() {
    BOOST_TEST_EQ(number_of<double>("0.1"), 0.1);
    BOOST_TEST_EQ(number_of<double>("1.005"), 1.005);
    BOOST_TEST_EQ(number_of<double>("0.07"), 0.07);
    BOOST_TEST_EQ(number_of<double>("1.5"), 1.5);
    BOOST_TEST_EQ(number_of<double>("12.034"), 12.034);
    BOOST_TEST_EQ(number_of<double>("3.14159"), 3.14159);
    BOOST_TEST_EQ(number_of<double>("0.123"), 0.123);
}

template <class Number>
void a_point_without_a_digit_after_it_is_invalid() {
    using calc::kind;
    lexes_to<Number>("3.", {
                               {kind::number, "3", Number(3)},
                               {kind::invalid, ".", Number(0)},
                           });
    lexes_to<Number>("3.+1", {
                                 {kind::number, "3", Number(3)},
                                 {kind::invalid, ".", Number(0)},
                                 {kind::plus, "+", Number(0)},
                                 {kind::number, "1", Number(1)},
                             });
    lexes_to<Number>(".5", {
                               {kind::invalid, ".", Number(0)},
                               {kind::number, "5", Number(5)},
                           });
}

template <class Number>
void an_unknown_character_is_one_invalid_token() {
    using calc::kind;
    lexes_to<Number>("$", {
                              {kind::invalid, "$", Number(0)},
                          });
    lexes_to<Number>("1 $@ 2", {
                                   {kind::number, "1", Number(1)},
                                   {kind::invalid, "$", Number(0)},
                                   {kind::invalid, "@", Number(0)},
                                   {kind::number, "2", Number(2)},
                               });
    lexes_to<Number>("!", {
                              {kind::invalid, "!", Number(0)},
                          });
}

/** A non-ASCII character, `e` with an acute accent in UTF-8, is one invalid token per byte. */
template <class Number>
void a_non_ascii_character_is_one_invalid_token_per_byte() {
    using calc::kind;
    lexes_to<Number>("\xC3\xA9", {
                                     {kind::invalid, "\xC3", Number(0)},
                                     {kind::invalid, "\xA9", Number(0)},
                                 });
}

/** Whether `x` is infinite, by the two-step call that finds boost::decimal's isinf or std's. */
template <class Number>
bool is_infinite(const Number& x) {
    using std::isinf;
    return isinf(x);
}

/**
 Digits past the ones the lexer keeps change nothing in a fraction: `1.` and four hundred zeros is
 exactly 1.
*/
template <class Number>
void digits_past_the_precision_do_not_change_a_fraction() {
    BOOST_TEST(number_of<Number>("1." + std::string(400, '0')) == Number(1));
    BOOST_TEST(number_of<Number>("0.5" + std::string(400, '0')) == Number(1) / Number(2));
}

/** Integer digits past the precision scale the value up: `1` and four hundred zeros is infinite. */
template <class Number>
void integer_digits_past_the_precision_scale_it_up() {
    BOOST_TEST(is_infinite(number_of<Number>("1" + std::string(400, '0'))));
    BOOST_TEST(number_of<Number>("1" + std::string(30, '0')) ==
               number_of<Number>("1000000000000000") * number_of<Number>("1000000000000000"));
}

/**
 A fraction of four hundred zeros and a 1 is 0, the nearest value to 1e-401 in both types: the
 smallest positive double is about 4.9e-324, and the smallest positive decimal64_t 1e-398.
*/
template <class Number>
void a_fraction_below_the_smallest_value_is_zero() {
    BOOST_TEST(number_of<Number>("0." + std::string(400, '0') + "1") == Number(0));
}

/**
 The value of `text` as std::strtod reads it into a double, all of it, in the "C" locale every
 test program runs in.

 Tip: not std::from_chars, which the libc++ of Xcode 16.4 does not provide for floating point.
 strtod is correctly rounded on every lane, and its errno is not read: it may set ERANGE for a
 subnormal result, which a test below reads on purpose.
*/
double double_from_text(std::string_view text) {
    const std::string terminated(text);
    char* end = nullptr;
    const double value = std::strtod(terminated.c_str(), &end);
    require(BOOST_TEST(end == terminated.c_str() + terminated.size()));
    return value;
}

/** The value of `text` as Boost.Decimal's from_chars reads it into a decimal64_t. */
decimal64_t decimal_from_chars(std::string_view text) {
    decimal64_t value{0};
    const auto read = boost::decimal::from_chars(text.data(), text.data() + text.size(), value);
    require(BOOST_TEST(read.ec == std::errc{}));
    return value;
}

/** The thirty-digit integer of the two tests below. */
constexpr std::string_view thirty_digits = "123456789012345678901234567890";

/** A literal of thirty digits is within one unit in the last place of the nearest double. */
void a_long_integer_is_within_one_ulp_in_double() {
    const double nearest = double_from_text(thirty_digits);
    const auto got = number_of<double>(thirty_digits);
    BOOST_TEST(got >= std::nextafter(nearest, 0.0));
    BOOST_TEST(got <= std::nextafter(nearest, std::numeric_limits<double>::infinity()));
}

/** A literal of thirty digits is within one unit in the last place of the nearest decimal64_t. */
void a_long_integer_is_within_one_ulp_in_decimal() {
    using boost::decimal::nextafter;
    const decimal64_t nearest = decimal_from_chars(thirty_digits);
    const auto got = number_of<decimal64_t>(thirty_digits);
    BOOST_TEST(got >= nextafter(nearest, decimal64_t(0)));
    BOOST_TEST(got <= nextafter(nearest, std::numeric_limits<decimal64_t>::infinity()));
}

/**
 A fraction whose value is a subnormal lexes to it, though ten to the number of its digits is
 infinite: 1e-321 in double, 1e-391 in decimal64_t.
*/
void a_subnormal_fraction_lexes_to_its_value() {
    const std::string binary = "0." + std::string(320, '0') + "1";
    BOOST_TEST_EQ(number_of<double>(binary), double_from_text(binary));
    BOOST_TEST(number_of<decimal64_t>("0." + std::string(390, '0') + "1") == decimal64_t(1, -391));
}

/** `x` written out in full, every digit of its integer part, as std::to_chars writes it. */
std::string in_full(double x) {
    std::string text(400, '\0');
    const std::to_chars_result written =
        std::to_chars(text.data(), text.data() + text.size(), x, std::chars_format::fixed);
    require(BOOST_TEST(written.ec == std::errc{}));
    text.resize(static_cast<std::size_t>(written.ptr - text.data()));
    return text;
}

/** Whether `got` is finite and at most four units in the last place from `nearest`. */
bool within_four_ulps(double got, double nearest) {
    double below = nearest;
    double above = nearest;
    for (int i = 0; i < 4; ++i) {
        below = std::nextafter(below, -std::numeric_limits<double>::infinity());
        above = std::nextafter(above, std::numeric_limits<double>::infinity());
    }
    return std::isfinite(got) && below <= got && got <= above;
}

/**
 A literal at the top of double's range is finite, near the nearest double, though its digits are
 scaled up by ten to the 292nd: the largest double written out in full, all 309 digits, and a
 literal of 17 digits and 292 zeros a few units below it.
*/
void a_literal_at_the_top_of_the_range_is_finite_in_double() {
    const std::string largest = in_full(std::numeric_limits<double>::max());
    require(BOOST_TEST_EQ(largest.size(), 309U));
    BOOST_TEST(within_four_ulps(number_of<double>(largest), std::numeric_limits<double>::max()));
    const std::string below = "17976931348623146" + std::string(292, '0');
    BOOST_TEST(within_four_ulps(number_of<double>(below), double_from_text(below)));
}

/** A literal just past the largest double, `1` and 309 zeros, is infinite. */
void a_literal_past_the_top_of_the_range_is_infinite_in_double() {
    BOOST_TEST(is_infinite(number_of<double>("1" + std::string(309, '0'))));
}

/** The largest decimal64_t written out in full, 16 nines and 369 zeros, lexes to it exactly. */
void the_largest_decimal_lexes_to_it_exactly() {
    BOOST_TEST(number_of<decimal64_t>(std::string(16, '9') + std::string(369, '0')) ==
               std::numeric_limits<decimal64_t>::max());
}

template <class Number>
void spaces_tabs_and_line_ends_are_whitespace() {
    using calc::kind;
    lexes_to<Number>("\t1\r\n+ \t2\r\n", {
                                             {kind::number, "1", Number(1)},
                                             {kind::plus, "+", Number(0)},
                                             {kind::number, "2", Number(2)},
                                         });
    lexes_to<Number>("", {});
    lexes_to<Number>(" \t\r\n", {});
}

template <class Number>
void the_end_token_stays() {
    calc::stream<Number> lexer("7");
    BOOST_TEST(!lexer.at_end());
    BOOST_TEST(lexer.advance().value == Number(7));
    for (int i = 0; i < 3; ++i) {
        BOOST_TEST(lexer.at_end());
        BOOST_TEST_EQ(name_of(lexer.peek().type), "end");
        BOOST_TEST_EQ(name_of(lexer.advance().type), "end");
    }
    BOOST_TEST(lexer.at_end());
}

template <class Number>
void peek_does_not_consume() {
    calc::stream<Number> lexer("x+");
    BOOST_TEST_EQ(lexer.peek().text, "x");
    BOOST_TEST_EQ(lexer.peek().text, "x");
    BOOST_TEST_EQ(lexer.advance().text, "x");
    BOOST_TEST_EQ(lexer.peek().text, "+");
}

/** A token's text is a view into the source, which the stream never copies. */
template <class Number>
void a_text_points_into_the_source() {
    const std::string_view source = "ab + 12";
    calc::stream<Number> lexer(source);
    BOOST_TEST(lexer.advance().text.data() == source.data());
    BOOST_TEST(lexer.advance().text.data() == source.data() + 3);
    BOOST_TEST(lexer.advance().text.data() == source.data() + 5);
}

template <class Number>
void every_case() {
    an_assignment_lexes_with_its_texts_and_values<Number>();
    every_two_character_operator_is_one_token<Number>();
    every_one_character_operator_is_one_token<Number>();
    a_name_is_letters_digits_and_underscores<Number>();
    a_point_without_a_digit_after_it_is_invalid<Number>();
    an_unknown_character_is_one_invalid_token<Number>();
    a_non_ascii_character_is_one_invalid_token_per_byte<Number>();
    spaces_tabs_and_line_ends_are_whitespace<Number>();
    the_end_token_stays<Number>();
    peek_does_not_consume<Number>();
    a_text_points_into_the_source<Number>();
    digits_past_the_precision_do_not_change_a_fraction<Number>();
    integer_digits_past_the_precision_scale_it_up<Number>();
    a_fraction_below_the_smallest_value_is_zero<Number>();
}

}  // namespace

int main() {
    every_case<decimal64_t>();
    every_case<double>();
    a_decimal_fraction_is_exact_in_decimal();
    a_double_fraction_is_the_nearest_double();
    a_long_integer_is_within_one_ulp_in_double();
    a_long_integer_is_within_one_ulp_in_decimal();
    a_subnormal_fraction_lexes_to_its_value();
    a_literal_at_the_top_of_the_range_is_finite_in_double();
    a_literal_past_the_top_of_the_range_is_infinite_in_double();
    the_largest_decimal_lexes_to_it_exactly();
    return boost::report_errors();
}
