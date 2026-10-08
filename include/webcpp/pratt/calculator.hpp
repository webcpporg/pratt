// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The calculator: a grammar of numbers, variables and C's arithmetic and
 comparison operators on the engine, over any type that models
 @ref webcpp::pratt::calc::number, `boost::decimal::decimal64_t` by default.
*/
#ifndef WEBCPP_PRATT_CALCULATOR_HPP
#define WEBCPP_PRATT_CALCULATOR_HPP

#include <webcpp/pratt/parser.hpp>

#include <boost/decimal.hpp>

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <string_view>

namespace webcpp::pratt::calc {

namespace detail::math {

// The two-step call: a number type's own overload, found by ADL (boost::decimal's), or std's for
// a builtin type, as `using std::pow; pow(a, b);` finds it.
using std::fmod;
using std::pow;
using std::trunc;

/** `pow(a, b)`, as the two-step call finds it. */
template <class N>
auto adl_pow(const N& a, const N& b) -> decltype(pow(a, b)) {
    return pow(a, b);
}

/** `fmod(a, b)`, as the two-step call finds it. */
template <class N>
auto adl_fmod(const N& a, const N& b) -> decltype(fmod(a, b)) {
    return fmod(a, b);
}

/** `trunc(a)`, as the two-step call finds it. */
template <class N>
auto adl_trunc(const N& a) -> decltype(trunc(a)) {
    return trunc(a);
}

}  // namespace detail::math

/**
 A type the calculator computes in.

 It is copyable and constructible from `int`; it has the binary `+ - * /`,
 the unary `-` and the six comparisons; and `pow(a, b)`, `fmod(a, b)` and
 `trunc(a)`, found by ADL after `using std::pow; using std::fmod;
 using std::trunc;`, so `boost::decimal`'s overloads serve its types and
 std's serve the builtin floating-point types.

 @note The three functions must return `N` itself, so that the calculator
 never computes through another type. `int` does not model it: std's
 `pow`, `fmod` and `trunc` of an `int` return a `double`.
*/
template <class N>
concept number =
    std::copyable<N> && std::constructible_from<N, int> && requires(const N a, const N b) {
        { a + b } -> std::convertible_to<N>;
        { a - b } -> std::convertible_to<N>;
        { a * b } -> std::convertible_to<N>;
        { a / b } -> std::convertible_to<N>;
        { -a } -> std::convertible_to<N>;
        { a == b } -> std::convertible_to<bool>;
        { a != b } -> std::convertible_to<bool>;
        { a < b } -> std::convertible_to<bool>;
        { a <= b } -> std::convertible_to<bool>;
        { a > b } -> std::convertible_to<bool>;
        { a >= b } -> std::convertible_to<bool>;
        { detail::math::adl_pow(a, b) } -> std::same_as<N>;
        { detail::math::adl_fmod(a, b) } -> std::same_as<N>;
        { detail::math::adl_trunc(a) } -> std::same_as<N>;
    };

/** What a token is. */
enum class kind {
    number,
    name,
    plus,
    minus,
    star,
    slash,
    percent,
    caret,
    lparen,
    rparen,
    assign,
    eq,
    ne,
    lt,
    le,
    gt,
    ge,
    increment,
    decrement,
    /** A character no token starts with, one character long. */
    invalid,
    /** The end of the input, which an empty text at the end of the source spells. */
    end,
};

/**
 A token of the calculator's input.

 @tparam Number The type a number token's value is.
*/
template <number Number>
struct token {
    /** What the token is. */
    kind k = kind::end;

    /** The value of a number token; 0 for every other kind. */
    Number value{0};

    /** The token's characters, a view into the source. */
    std::string_view text{};
};

namespace detail {

/** An operator spelled with two characters, and its kind. */
struct two_character_operator {
    std::string_view text;
    kind k;
};

/** Every operator spelled with two characters, which the lexer tries before one character. */
inline constexpr std::array two_character_operators{
    two_character_operator{.text = "++", .k = kind::increment},
    two_character_operator{.text = "--", .k = kind::decrement},
    two_character_operator{.text = "==", .k = kind::eq},
    two_character_operator{.text = "!=", .k = kind::ne},
    two_character_operator{.text = "<=", .k = kind::le},
    two_character_operator{.text = ">=", .k = kind::ge},
};

/** The kind of the one-character operator `c`, or @ref kind::invalid when `c` is none. */
constexpr kind one_character_kind(char c) noexcept {
    switch (c) {
        case '+': return kind::plus;
        case '-': return kind::minus;
        case '*': return kind::star;
        case '/': return kind::slash;
        case '%': return kind::percent;
        case '^': return kind::caret;
        case '(': return kind::lparen;
        case ')': return kind::rparen;
        case '=': return kind::assign;
        case '<': return kind::lt;
        case '>': return kind::gt;
        default: return kind::invalid;
    }
}

constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

constexpr bool is_name_start(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool is_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}  // namespace detail

/**
 The calculator's lexer, a lazy @ref webcpp::pratt::token_stream. It holds
 the source, its position and one token of lookahead, and lexes the next
 token only when @ref advance consumes the current one.

 A number is digits, then optionally `.` and more digits; it is computed in
 `Number` arithmetic alone, never through `double`, so `0.1` is exactly one
 tenth in a decimal type. A name is a letter or `_`, then letters, digits
 and `_`. Space, tab, carriage return and line feed separate tokens. Any
 other character is a token of kind @ref kind::invalid, one character long,
 which the grammar rejects where it stands.

 @tparam Number The type a number token's value is.
*/
template <number Number>
class stream {
public:
    /** The tokens the stream yields. */
    using token_type = token<Number>;

    /**
     Constructs the stream of `source`, which must outlive it and its
     tokens, and lexes the first token.
    */
    explicit stream(std::string_view source) : source_(source), next_(lex()) {}

    /** The next token, not consumed; the end token once the input is spent. */
    [[nodiscard]] const token_type& peek() const { return next_; }

    /** Consumes the next token and returns it; at the end, returns the end token again. */
    token_type advance() {
        token_type current = next_;
        if (current.k == kind::end) {
            return current;
        }
        next_ = lex();
        return current;
    }

    /** Whether the input is spent: the next token is the end token. */
    [[nodiscard]] bool at_end() const { return next_.k == kind::end; }

private:
    /** Lexes the token at the position, after the whitespace before it, and moves past it. */
    token_type lex() {
        skip_whitespace();
        if (pos_ >= source_.size()) {
            return token_type{.k = kind::end, .value = Number(0), .text = source_.substr(pos_)};
        }
        const char c = source_[pos_];
        if (detail::is_digit(c)) {
            return lex_number();
        }
        if (detail::is_name_start(c)) {
            return lex_name();
        }
        return lex_operator();
    }

    void skip_whitespace() {
        while (pos_ < source_.size() && detail::is_whitespace(source_[pos_])) {
            ++pos_;
        }
    }

    /**
     Lexes a number at the position, which is a digit.

     Tip: the fraction's digits continue the mantissa, which is then divided
     once by ten to the number of those digits. While the mantissa and the
     divisor are exact in `Number`, that division is the one rounding, so a
     decimal type gets the exact value and `double` the nearest one, the
     literal's; a division by ten per digit would round once per digit.
    */
    token_type lex_number() {
        const std::size_t start = pos_;
        Number mantissa(0);
        read_digits(mantissa);
        const bool has_fraction = pos_ + 1 < source_.size() && source_[pos_] == '.' &&
                                  detail::is_digit(source_[pos_ + 1]);
        if (!has_fraction) {
            return number_token(start, mantissa);
        }
        ++pos_;
        const Number scale = read_digits(mantissa);
        return number_token(start, Number(mantissa / scale));
    }

    /**
     Appends the digits at the position to `mantissa` and returns ten to the
     number of them.
    */
    Number read_digits(Number& mantissa) {
        const Number ten(10);
        Number scale(1);
        while (pos_ < source_.size() && detail::is_digit(source_[pos_])) {
            mantissa = (mantissa * ten) + Number(source_[pos_] - '0');
            scale = scale * ten;
            ++pos_;
        }
        return scale;
    }

    /** The number token of `value`, whose text runs from `start` to the position. */
    [[nodiscard]] token_type number_token(std::size_t start, const Number& value) const {
        return token_type{
            .k = kind::number,
            .value = value,
            .text = source_.substr(start, pos_ - start),
        };
    }

    /** Lexes a name at the position, which is a letter or `_`. */
    token_type lex_name() {
        const std::size_t start = pos_;
        while (pos_ < source_.size() &&
               (detail::is_name_start(source_[pos_]) || detail::is_digit(source_[pos_]))) {
            ++pos_;
        }
        return token_type{
            .k = kind::name,
            .value = Number(0),
            .text = source_.substr(start, pos_ - start),
        };
    }

    /** Lexes an operator of two characters, one, or an invalid character. */
    token_type lex_operator() {
        const std::string_view pair = source_.substr(pos_, 2);
        for (const detail::two_character_operator& candidate : detail::two_character_operators) {
            if (pair == candidate.text) {
                pos_ += 2;
                return token_type{.k = candidate.k, .value = Number(0), .text = pair};
            }
        }
        const std::string_view one = source_.substr(pos_, 1);
        ++pos_;
        return token_type{
            .k = detail::one_character_kind(one.front()),
            .value = Number(0),
            .text = one,
        };
    }

    std::string_view source_;
    std::size_t pos_ = 0;
    token_type next_;
};

}  // namespace webcpp::pratt::calc

#endif  // WEBCPP_PRATT_CALCULATOR_HPP
