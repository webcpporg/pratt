// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests the calculator over decimal64_t and over double: the reference session's values and
// errors, exact integral powers, division by zero, assignment and increment through parentheses,
// an invalid character wherever it stands, the depth limit, and the environments a user brings,
// one in fixed storage whose assign fails and one whose allocator counts.

#include <webcpp/pratt/calculator.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/decimal.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/system/errc.hpp>

#include <cmath>
#include <concepts>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <type_traits>

#include "environments.hpp"

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;

using boost::decimal::decimal64_t;
using webcpp::test::allocation_count;
using webcpp::test::counting;
using webcpp::test::fixed_environment;

namespace {

/** An environment whose names all hold one value and refuse every write. */
template <class Number>
class read_only_environment {
public:
    using number_type = Number;

    [[nodiscard]] const Number* lookup(std::string_view /*name*/) const { return &value_; }

    static pratt::result<void> assign(std::string_view /*name*/, const Number& /*value*/) {
        return boost::system::errc::make_error_code(boost::system::errc::read_only_file_system);
    }

private:
    Number value_{7};
};

/** A type that reads names but cannot write them. */
struct lookup_only {
    using number_type = double;

    [[nodiscard]] static const double* lookup(std::string_view /*name*/) { return nullptr; }
};

}  // namespace

/**
 Boost.Multiprecision's decimal of 50 digits, without expression templates: a number that
 pratt::error converts to, as its constructor from any arithmetic type takes an enum too.
*/
using decimal50 = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<50>,
                                                boost::multiprecision::et_off>;

static_assert(calc::number<decimal50>);
static_assert(std::is_convertible_v<pratt::error, decimal50>);
static_assert(std::same_as<calc::environment<>::number_type, decimal64_t>);
static_assert(std::same_as<calc::environment<double>::number_type, double>);
static_assert(calc::environment_for<calc::environment<>>);
static_assert(calc::environment_for<calc::environment<double>>);
static_assert(calc::environment_for<calc::environment<decimal64_t, counting<char>>>);
static_assert(calc::environment_for<fixed_environment<decimal64_t>>);
static_assert(calc::environment_for<read_only_environment<double>>);
static_assert(!calc::environment_for<lookup_only>);
static_assert(!calc::environment_for<int>);
static_assert(pratt::grammar_for<calc::grammar<calc::environment<>>, calc::stream<decimal64_t>>);
static_assert(pratt::grammar_for<calc::grammar<calc::environment<double>>, calc::stream<double>>);
static_assert(
    pratt::grammar_for<calc::grammar<fixed_environment<decimal64_t>>, calc::stream<decimal64_t>>);

namespace {

/** Names longer than any small-string buffer, so that a key of one allocates. */
constexpr std::string_view first_long_name = "a_name_longer_than_any_small_string_buffer_one";
constexpr std::string_view second_long_name = "a_name_longer_than_any_small_string_buffer_two";
constexpr std::string_view third_long_name = "a_name_longer_than_any_small_string_buffer_six";

/** Checks that `source` evaluates over `env` to `expected`. */
template <class Environment>
void value_is(Environment& env, std::string_view source,
              const typename Environment::number_type& expected) {
    const auto got = calc::evaluate(source, env);
    if (!BOOST_TEST(got.has_value())) {
        std::cerr << "  in: " << source << " (" << got.error().message() << ")\n";
        return;
    }
    if (!BOOST_TEST(*got == expected)) {
        std::cerr << std::setprecision(17) << "  in: " << source << " (got " << *got
                  << ", expected " << expected << ")\n";
    }
}

/** Checks that `source` evaluates over `env` to the error `expected`. */
template <class Environment>
void error_is(Environment& env, std::string_view source, boost::system::error_code expected) {
    const auto got = calc::evaluate(source, env);
    if (!BOOST_TEST(got.has_error())) {
        std::cerr << "  in: " << source << " (got " << *got << ")\n";
        return;
    }
    if (!BOOST_TEST_EQ(got.error(), expected)) {
        std::cerr << "  in: " << source << '\n';
    }
}

/** The value of `name` in `env`, which must hold it. */
template <class Environment>
Environment::number_type value_of(const Environment& env, std::string_view name) {
    const typename Environment::number_type* found = env.lookup(name);
    if (!BOOST_TEST(found != nullptr)) {
        std::cerr << "  name: " << name << '\n';
        return typename Environment::number_type(0);
    }
    return *found;
}

/**
 The reference session, its eighteen values and its eight errors, in one environment.

 @param b What `b = a / 2 - 100` gives, 1048503.5 written in `Number`.
*/
template <class Number>
void the_reference_session(const Number& b) {
    calc::environment<Number> env;
    value_is(env, "a = 43 + 12 + 2 * ( 44 - 12 ) ^ 4", Number(2097207));
    value_is(env, "a", Number(2097207));
    value_is(env, "2 ^ 3 ^ 2", Number(512));
    value_is(env, "-2 ^ 2", Number(-4));
    value_is(env, "b = a / 2 - 100", b);
    value_is(env, "b > 1000000", Number(1));
    value_is(env, "b == a / 2 - 100", Number(1));
    value_is(env, "10 % 3", Number(1));
    value_is(env, "i = 0", Number(0));
    value_is(env, "i++", Number(0));
    value_is(env, "i", Number(1));
    value_is(env, "++i", Number(2));
    value_is(env, "--i", Number(1));
    value_is(env, "i--", Number(1));
    value_is(env, "i", Number(0));
    value_is(env, "c = d = 5", Number(5));
    value_is(env, "c + d", Number(10));
    value_is(env, "1.5 * 2", Number(3));

    error_is(env, "x + 1", pratt::error::undefined_variable);
    error_is(env, "5 = 3", pratt::error::invalid_assignment_target);
    error_is(env, "3++", pratt::error::invalid_assignment_target);
    error_is(env, "( 1 + 2", pratt::error::expected_closing_token);
    error_is(env, "1 2", pratt::error::trailing_input);
    error_is(env, "1 +", pratt::error::unexpected_end);
    error_is(env, "1 $ 2", pratt::error::invalid_character);
    error_is(env, "", pratt::error::unexpected_end);
}

/** An integral exponent multiplies in `Number`: exact while the result fits its precision. */
template <class Number>
void an_integral_power_is_exact() {
    calc::environment<Number> env;
    value_is(env, "(44 - 12) ^ 4", Number(1048576));
    value_is(env, "2 ^ 53", Number(9007199254740992));
    value_is(env, "2 ^ -3", Number(1) / Number(8));
    value_is(env, "10 ^ -2", Number(1) / Number(100));
    value_is(env, "(-2) ^ 3", Number(-8));
    value_is(env, "(-2) ^ -3", Number(-1) / Number(8));
    value_is(env, "1.5 ^ 2", Number(9) / Number(4));
    value_is(env, "3 ^ 1", Number(3));
    value_is(env, "1 ^ (10 ^ 300)", Number(1));
    // Past int's range: Boost.Decimal's pow answers NaN for a negative base there.
    value_is(env, "(-1) ^ 10000000000", Number(1));
    value_is(env, "(-1) ^ 10000000001", Number(-1));
}

/**
 A negative integral power whose positive power overflows, or underflows to 0, is still the
 nearest value: `pow` computes it, as no square can.

 @param tiny The smallest positive value of `Number`, a subnormal.
 @param tiny_exponent The integral power of two, negative, that is `tiny` in binary, or of ten in
 decimal.
*/
template <class Number>
void a_negative_power_past_the_range_is_still_computed(const Number& tiny,
                                                       std::string_view tiny_exponent) {
    calc::environment<Number> env;
    value_is(env, tiny_exponent, tiny);
    const auto huge = calc::evaluate("0.1 ^ -400", env);
    if (!BOOST_TEST(huge.has_value())) {
        std::cerr << "  in: 0.1 ^ -400 (" << huge.error().message() << ")\n";
        return;
    }
    BOOST_TEST(*huge > std::numeric_limits<Number>::max());
}

/** Zero to the power zero is one, as every number to the power zero is. */
template <class Number>
void zero_to_the_zero_is_one() {
    calc::environment<Number> env;
    value_is(env, "0 ^ 0", Number(1));
    value_is(env, "x = 0", Number(0));
    value_is(env, "x ^ 0", Number(1));
    value_is(env, "7 ^ 0", Number(1));
    value_is(env, "0 ^ 2", Number(0));
}

/** Zero to a negative power is one divided by zero. */
template <class Number>
void zero_to_a_negative_power_divides_by_zero() {
    calc::environment<Number> env;
    error_is(env, "0 ^ -1", pratt::error::division_by_zero);
    error_is(env, "0 ^ -2", pratt::error::division_by_zero);
}

/**
 In decimal, the parity of an exponent past the type's precision is even, as every such integer is
 a multiple of ten, and the sign of the power follows it.

 Tip: 9999999999999999 * 10 ^ 368 halves inexactly in 16 digits, and Boost.Decimal's fmod of it by
 2 is not 0; and 9999999999999999 is odd, though its half rounds to 5 * 10 ^ 15.
*/
void the_parity_of_a_huge_exponent_is_exact_in_decimal() {
    calc::environment<decimal64_t> env;
    value_is(env, "(-1) ^ (9999999999999999 * 10 ^ 368)", decimal64_t(1));
    value_is(env, "(-1) ^ 9999999999999999", decimal64_t(-1));
    value_is(env, "(-1) ^ 9999999999999998", decimal64_t(1));
    value_is(env, "(-2) ^ -1", decimal64_t(-5, -1));
}

/** In double, a negative power past the range keeps the sign an odd exponent gives it. */
void a_negative_base_keeps_its_sign_past_the_range() {
    calc::environment<double> env;
    value_is(env, "(-2) ^ -1073", -2 * std::numeric_limits<double>::denorm_min());
    value_is(env, "(-2) ^ -1074", std::numeric_limits<double>::denorm_min());
}

/** 2^60 has 19 digits: exact in double, and in decimal64_t its 16 digits, rounded once. */
void two_to_the_sixty_is_exact() {
    calc::environment<double> binary;
    value_is(binary, "2 ^ 60", 1152921504606846976.0);
    value_is(binary, "2 ^ 1000", std::ldexp(1.0, 1000));
    calc::environment<decimal64_t> decimal;
    value_is(decimal, "2 ^ 60", decimal64_t(1152921504606847, 3));
    value_is(decimal, "2 ^ 60 == 1152921504606846976", decimal64_t(1));
}

/**
 In double too, an integral power is repeated squaring, not std::pow.

 Tip: squaring gives 1.1 ^ 50 as 0x1.d5903bbcbf5cbp+6 (117.39085287969571), the same on every
 lane, since each of its products is one correctly rounded IEEE multiplication; std::pow gives
 0x1.d5903bbcbf5dp+6 (117.39085287969579), five units in the last place away. Both were printed
 with %a from this program's own arithmetic.
*/
void an_integral_power_squares_in_double() {
    calc::environment<double> env;
    value_is(env, "1.1 ^ 50", 0x1.d5903bbcbf5cbp+6);
}

/** A literal with 15 fraction digits keeps them through the arithmetic, in decimal. */
void a_long_fraction_keeps_its_digits() {
    calc::environment<decimal64_t> env;
    value_is(env, "0.123456789012345 * 1", decimal64_t(123456789012345, -15));
    value_is(env, "0.1 + 0.2", decimal64_t(3, -1));
}

/**
 A non-integral exponent goes through `pow`, found as the number type's own.

 @param low A value just below the square root of 2.
 @param high A value just above it.
*/
template <class Number>
void a_fractional_power_goes_through_pow(const Number& low, const Number& high) {
    using std::pow;
    calc::environment<Number> env;
    const auto got = calc::evaluate("2 ^ 0.5", env);
    if (!BOOST_TEST(got.has_value())) {
        return;
    }
    BOOST_TEST(*got > low);
    BOOST_TEST(*got < high);
    BOOST_TEST(*got == pow(Number(2), Number(1) / Number(2)));
}

/** An infinite exponent is not integral: it goes through `pow`, and the evaluation ends. */
template <class Number>
void an_infinite_exponent_goes_through_pow() {
    calc::environment<Number> env;
    value_is(env, "0.5 ^ (10 ^ 400)", Number(0));
    value_is(env, "2 ^ (10 ^ 400) == 10 ^ 400", Number(1));
}

template <class Number>
void a_zero_divisor_is_an_error() {
    calc::environment<Number> env;
    error_is(env, "1 / 0", pratt::error::division_by_zero);
    error_is(env, "5 % 0", pratt::error::division_by_zero);
    error_is(env, "1 / (2 - 2)", pratt::error::division_by_zero);
    error_is(env, "0 / 0", pratt::error::division_by_zero);
    error_is(env, "5 % 0.0", pratt::error::division_by_zero);
    value_is(env, "0 / 5", Number(0));
    value_is(env, "7.5 % 2", Number(3) / Number(2));
    value_is(env, "-7 % 3", Number(-1));
}

template <class Number>
void the_comparisons_answer_one_or_zero() {
    calc::environment<Number> env;
    value_is(env, "1 == 1", Number(1));
    value_is(env, "1 == 2", Number(0));
    value_is(env, "1 != 2", Number(1));
    value_is(env, "1 != 1", Number(0));
    value_is(env, "1 < 2", Number(1));
    value_is(env, "2 < 1", Number(0));
    value_is(env, "2 <= 2", Number(1));
    value_is(env, "3 <= 2", Number(0));
    value_is(env, "2 > 1", Number(1));
    value_is(env, "1 > 2", Number(0));
    value_is(env, "2 >= 2", Number(1));
    value_is(env, "1 >= 2", Number(0));
    value_is(env, "1 + 1 == 2", Number(1));
}

/** A grouping keeps a bare name, so it can be assigned and incremented; a number cannot. */
template <class Number>
void parentheses_keep_the_name() {
    calc::environment<Number> env;
    value_is(env, "(a) = 1", Number(1));
    BOOST_TEST(value_of(env, "a") == Number(1));
    value_is(env, "(a)++", Number(1));
    BOOST_TEST(value_of(env, "a") == Number(2));
    value_is(env, "++((a))", Number(3));
    value_is(env, "((a)) = 10", Number(10));
    BOOST_TEST(value_of(env, "a") == Number(10));
    error_is(env, "(1) = 2", pratt::error::invalid_assignment_target);
    error_is(env, "(a + 1) = 2", pratt::error::invalid_assignment_target);
    error_is(env, "(a = 1) = 2", pratt::error::invalid_assignment_target);
    error_is(env, "(1)++", pratt::error::invalid_assignment_target);
    error_is(env, "++(1)", pratt::error::invalid_assignment_target);
}

/** A name nothing has written errors when it is read, and a failed write leaves it unwritten. */
template <class Number>
void an_undefined_name_errors_when_read() {
    calc::environment<Number> env;
    error_is(env, "x", pratt::error::undefined_variable);
    error_is(env, "-x", pratt::error::undefined_variable);
    error_is(env, "x++", pratt::error::undefined_variable);
    error_is(env, "++x", pratt::error::undefined_variable);
    error_is(env, "x = y", pratt::error::undefined_variable);
    BOOST_TEST(env.lookup("x") == nullptr);
    value_is(env, "x = 2", Number(2));
    value_is(env, "x = x * 3", Number(6));
    value_is(env, "+x", Number(6));
}

/**
 An invalid character, where no earlier error stops the parse, gives invalid_character: not
 trailing_input after a complete expression, nor unexpected_end where an operand is missing.
*/
template <class Number>
void an_invalid_character_is_neither_trailing_input_nor_an_unexpected_end() {
    calc::environment<Number> env;
    error_is(env, "1 $ 2", pratt::error::invalid_character);
    error_is(env, "$", pratt::error::invalid_character);
    error_is(env, "(1 $", pratt::error::invalid_character);
    error_is(env, "1 +$", pratt::error::invalid_character);
    error_is(env, "1 $", pratt::error::invalid_character);
    error_is(env, "a = 1 $", pratt::error::invalid_character);
    error_is(env, "2 ^ 3 $", pratt::error::invalid_character);
    BOOST_TEST(env.lookup("a") == nullptr);
}

/**
 The first error in reading order is the one returned, and a write made before it stays made.

 Tip: `=` checks its target before it reads its right operand, and `i++` has written `i` before
 the `$` after it is read.
*/
template <class Number>
void the_first_error_in_reading_order_wins_and_earlier_writes_stay() {
    calc::environment<Number> env;
    error_is(env, "5 = 3 $", pratt::error::invalid_assignment_target);
    value_is(env, "i = 1", Number(1));
    error_is(env, "i++ $", pratt::error::invalid_character);
    BOOST_TEST(value_of(env, "i") == Number(2));
    error_is(env, "i++ )", pratt::error::trailing_input);
    BOOST_TEST(value_of(env, "i") == Number(3));
    // An operator finds its own error only once it has its operands, after a later character.
    BOOST_TEST(env.lookup("x") == nullptr);
    error_is(env, "x + 1 $", pratt::error::invalid_character);
    error_is(env, "1 / 0 $", pratt::error::invalid_character);
}

/** `evaluate` hands its depth limit to the engine: `((1))` makes three expression calls. */
template <class Number>
void evaluate_passes_its_depth_limit() {
    calc::environment<Number> env;
    const auto shallow = calc::evaluate("((1))", env, 2);
    BOOST_TEST(shallow.has_error() && shallow.error() == pratt::error::nesting_too_deep);
    const auto enough = calc::evaluate("((1))", env, 3);
    BOOST_TEST(enough.has_value() && *enough == Number(1));
}

/** The fixed-storage environment reads and writes two names, and refuses a third. */
void a_fixed_environment_returns_its_own_error() {
    fixed_environment<decimal64_t> env;
    value_is(env, "p = 2", decimal64_t(2));
    value_is(env, "q = p * 3", decimal64_t(6));
    value_is(env, "p + q == 8", decimal64_t(1));
    value_is(env, "p++", decimal64_t(2));
    value_is(env, "--q", decimal64_t(5));
    value_is(env, "p = q = 1.5", decimal64_t(15, -1));
    error_is(env, "r = 1",
             boost::system::errc::make_error_code(boost::system::errc::not_enough_memory));
    BOOST_TEST(env.lookup("r") == nullptr);
    error_is(env, "r++", pratt::error::undefined_variable);
    value_is(env, "p + q", decimal64_t(3));
}

/** Every write, `=` and the four increments, returns what the environment's assign returns. */
void a_failing_assign_is_returned_unchanged() {
    read_only_environment<double> env;
    const boost::system::error_code refused =
        boost::system::errc::make_error_code(boost::system::errc::read_only_file_system);
    value_is(env, "n + 1", 8.0);
    error_is(env, "n = 1", refused);
    error_is(env, "n++", refused);
    error_is(env, "n--", refused);
    error_is(env, "++n", refused);
    error_is(env, "--n", refused);
    error_is(env, "1 + (n = 2)", refused);
}

/**
 calc::environment allocates its map's nodes and its keys through the user's allocator: a new name
 allocates, and a lookup, a replaced value and an evaluation allocate nothing.

 Tip: only what each step adds is compared, never a total: a standard library may also allocate
 through the allocator when the map is built (MSVC's head node, and in a debug build its container
 proxies), so the exact count is the library's. What every one holds is that the allocator gives
 back all it allocated.
*/
void the_environment_allocates_through_its_allocator() {
    allocation_count count;
    {
        using counted = calc::environment<decimal64_t, counting<char>>;
        static_assert(std::same_as<counted::allocator_type, counting<char>>);
        counted env{counting<char>(&count)};
        static_assert(noexcept(env.get_allocator()));
        static_assert(std::same_as<decltype(env.get_allocator()), counting<char>>);
        BOOST_TEST(env.get_allocator().count() == &count);
        for (const std::string_view name : {first_long_name, second_long_name, third_long_name}) {
            const std::size_t before = count.allocations;
            BOOST_TEST(env.assign(name, decimal64_t(1)).has_value());
            BOOST_TEST_GT(count.allocations, before);
        }
        const std::size_t named = count.allocations;
        BOOST_TEST(env.assign(second_long_name, decimal64_t(2)).has_value());
        BOOST_TEST_EQ(count.allocations, named);
        BOOST_TEST(value_of(env, second_long_name) == decimal64_t(2));
        BOOST_TEST(value_of(env, first_long_name) == decimal64_t(1));
        BOOST_TEST(env.lookup("absent") == nullptr);
        BOOST_TEST_EQ(count.allocations, named);
        BOOST_TEST(calc::evaluate(std::string(first_long_name) + " * 2 + 1", env).has_value());
        BOOST_TEST_EQ(count.allocations, named);
    }
    BOOST_TEST_EQ(count.deallocations, count.allocations);
}

/** An environment's lookup is transparent and its assign inserts or replaces. */
template <class Number>
void the_environment_inserts_or_replaces() {
    calc::environment<Number> env;
    BOOST_TEST(env.lookup("v") == nullptr);
    BOOST_TEST(env.assign("v", Number(1)).has_value());
    BOOST_TEST(value_of(env, "v") == Number(1));
    BOOST_TEST(env.assign("v", Number(2)).has_value());
    BOOST_TEST(value_of(env, "v") == Number(2));
    BOOST_TEST(env.assign("w", Number(3)).has_value());
    BOOST_TEST(value_of(env, "v") == Number(2));
    BOOST_TEST(value_of(env, "w") == Number(3));
}

/**
 A number that pratt::error converts to evaluates, its errors included: 2^100 is exact in 50
 digits, and a third rounds once.
*/
void a_number_an_error_converts_to_evaluates() {
    calc::environment<decimal50> env;
    value_is(env, "1 / 3 + 2 ^ 100",
             decimal50("1267650600228229401496703205376") + (decimal50(1) / decimal50(3)));
    error_is(env, "1 / 0", pratt::error::division_by_zero);
    error_is(env, "5 % 0", pratt::error::division_by_zero);
    error_is(env, "0 ^ -1", pratt::error::division_by_zero);
    error_is(env, "x", pratt::error::undefined_variable);
}

template <class Number>
void every_case() {
    an_integral_power_is_exact<Number>();
    zero_to_the_zero_is_one<Number>();
    zero_to_a_negative_power_divides_by_zero<Number>();
    an_infinite_exponent_goes_through_pow<Number>();
    a_zero_divisor_is_an_error<Number>();
    the_comparisons_answer_one_or_zero<Number>();
    parentheses_keep_the_name<Number>();
    an_undefined_name_errors_when_read<Number>();
    an_invalid_character_is_neither_trailing_input_nor_an_unexpected_end<Number>();
    the_first_error_in_reading_order_wins_and_earlier_writes_stay<Number>();
    evaluate_passes_its_depth_limit<Number>();
    the_environment_inserts_or_replaces<Number>();
}

}  // namespace

int main() {
    the_reference_session<decimal64_t>(decimal64_t(10485035, -1));
    the_reference_session<double>(1048503.5);
    every_case<decimal64_t>();
    every_case<double>();
    two_to_the_sixty_is_exact();
    a_negative_power_past_the_range_is_still_computed<double>(
        std::numeric_limits<double>::denorm_min(), "2 ^ -1074");
    a_negative_power_past_the_range_is_still_computed<decimal64_t>(decimal64_t(1, -385),
                                                                   "10 ^ -385");
    the_parity_of_a_huge_exponent_is_exact_in_decimal();
    a_negative_base_keeps_its_sign_past_the_range();
    an_integral_power_squares_in_double();
    a_long_fraction_keeps_its_digits();
    a_fractional_power_goes_through_pow<decimal64_t>(decimal64_t(141421356237309, -14),
                                                     decimal64_t(141421356237310, -14));
    a_fractional_power_goes_through_pow<double>(std::numbers::sqrt2 - 1e-12,
                                                std::numbers::sqrt2 + 1e-12);
    a_fixed_environment_returns_its_own_error();
    a_failing_assign_is_returned_unchanged();
    the_environment_allocates_through_its_allocator();
    a_number_an_error_converts_to_evaluates();
    return boost::report_errors();
}
