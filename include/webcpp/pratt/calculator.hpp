// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The calculator: a grammar of numbers, variables and C's arithmetic and
 comparison operators on the engine, over any type that models
 @ref webcpp::pratt::calc::number, `boost::decimal::decimal64_t` by default.

 @see "The calculator", in the guide.
*/
#ifndef WEBCPP_PRATT_CALCULATOR_HPP
#define WEBCPP_PRATT_CALCULATOR_HPP

#include <webcpp/pratt/config.hpp>

#include <webcpp/pratt/parser.hpp>

#include <boost/decimal.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace webcpp::pratt::calc {

namespace detail::math {

// The two-step call: a number type's own overload, found by ADL (boost::decimal's), or std's for
// a builtin type, as `using std::pow; pow(a, b);` finds it.

/** std's `fmod`, which the two-step call finds for a builtin type. */
using std::fmod;

/** std's `pow`, which the two-step call finds for a builtin type. */
using std::pow;

/** std's `trunc`, which the two-step call finds for a builtin type. */
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
 `trunc(a)`, found by ADL after `using std::pow;`, `using std::fmod;` and
 `using std::trunc;`, so `boost::decimal`'s overloads serve its types and
 std's serve the builtin floating-point types.

 @tparam N The type the calculator would compute in.
 @note The three functions must return exactly `N`, so that the calculator
 never computes through another type. `int` does not model it: std's
 `pow`, `fmod` and `trunc` of an `int` return a `double`. Nor does a type
 whose functions return a proxy, an expression template such as
 Boost.Multiprecision's with `et_on`; the same type with `et_off` does.
 @see "The calculator", in the guide.
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

/**
 What a token is.

 @see "The calculator", in the guide.
*/
enum class kind {
    /** A number literal, such as `42` or `0.5`, whose value the token holds. */
    number,

    /** A variable's name, such as `x` or `_total2`. */
    name,

    /** `+`, binary addition or a unary plus. */
    plus,

    /** `-`, binary subtraction or a unary minus. */
    minus,

    /** `*`, multiplication. */
    star,

    /** `/`, division. */
    slash,

    /** `%`, the remainder of a division. */
    percent,

    /** `^`, exponentiation. */
    caret,

    /** `(`, which opens a grouping. */
    lparen,

    /** `)`, which closes a grouping. */
    rparen,

    /** `=`, assignment. */
    assign,

    /** `==`, equal. */
    eq,

    /** `!=`, not equal. */
    ne,

    /** `<`, less than. */
    lt,

    /** `<=`, less than or equal. */
    le,

    /** `>`, greater than. */
    gt,

    /** `>=`, greater than or equal. */
    ge,

    /** `++`, a prefix or a postfix increment. */
    increment,

    /** `--`, a prefix or a postfix decrement. */
    decrement,

    /**
     A character the lexer reads as no token, one character long.

     A non-ASCII character is several bytes of UTF-8, each a token of this
     kind, so a parse stops at its first byte.
    */
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
    kind type = kind::end;

    /** The value of a number token; 0 for every other kind. */
    Number value{0};

    /** The token's characters, a view into the source. */
    std::string_view text{};
};

namespace detail {

/** An operator spelled with two characters, and its kind. */
struct two_character_operator {
    /** The operator's two characters. */
    std::string_view text;

    /** The operator's kind. */
    kind type;
};

/** Every operator spelled with two characters, which the lexer tries before one character. */
inline constexpr std::array two_character_operators{
    two_character_operator{.text = "++", .type = kind::increment},
    two_character_operator{.text = "--", .type = kind::decrement},
    two_character_operator{.text = "==", .type = kind::eq},
    two_character_operator{.text = "!=", .type = kind::ne},
    two_character_operator{.text = "<=", .type = kind::le},
    two_character_operator{.text = ">=", .type = kind::ge},
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

/** Whether `c` is an ASCII decimal digit. */
constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

/** Whether `c` may start a name: an ASCII letter or `_`. */
constexpr bool is_name_start(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/** Whether `c` separates tokens: space, tab, carriage return or line feed. */
constexpr bool is_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/**
 `base` to the power `n`, by repeated squaring in `Number`: exact while it
 fits the type's precision, and infinite past its range.
*/
template <number Number>
Number power_of(Number base, std::size_t n) {
    Number result(1);
    while (n != 0) {
        if (n % 2 != 0) {
            result = result * base;
        }
        n /= 2;
        if (n != 0) {
            base = base * base;
        }
    }
    return result;
}

/**
 A value held as the unevaluated sum `high + low` of two numbers of a
 binary type, `low` at most half a unit in the last place of `high`: about
 twice the type's precision.
*/
template <number Number>
struct twofold {
    /** The value rounded to the type's precision. */
    Number high;
    /** The rest of the value, below the last place of `high`. */
    Number low;
};

/**
 `a` as the sum of two halves of its significand, each of at most half the
 type's digits, so that the product of two halves is exact (Dekker's split).

 @pre `a` times 2 to the half of the type's digits is finite.
*/
template <number Number>
twofold<Number> split(Number a) {
    using limits = std::numeric_limits<Number>;
    const Number factor =
        power_of(Number(2), static_cast<std::size_t>((limits::digits + 1) / 2)) + Number(1);
    const Number scaled = factor * a;
    const Number rest = scaled - a;
    const Number high = scaled - rest;
    return {high, a - high};
}

/**
 `a * b` as its rounded product and the exact error of that rounding
 (Dekker's product).

 Tip: every operation is a statement of its own, so that no compiler
 contracts one into a fused multiply-add, which would make the error
 inexact differently on each target.
 @pre `a` and `b` meet the precondition of @ref split.
*/
template <number Number>
twofold<Number> exact_product(Number a, Number b) {
    const Number product = a * b;
    const twofold<Number> x = split(a);
    const twofold<Number> y = split(b);
    Number error = x.high * y.high;
    error = error - product;
    Number term = x.high * y.low;
    error = error + term;
    term = x.low * y.high;
    error = error + term;
    term = x.low * y.low;
    error = error + term;
    return {product, error};
}

/** The product of `x` and `y`, to about twice the type's precision. */
template <number Number>
twofold<Number> twofold_product(twofold<Number> x, twofold<Number> y) {
    const twofold<Number> product = exact_product(x.high, y.high);
    Number low = product.low;
    Number term = x.high * y.low;
    low = low + term;
    term = x.low * y.high;
    low = low + term;
    const Number high = product.high + low;
    const Number carried = high - product.high;
    low = low - carried;
    return {high, low};
}

/**
 `base` to the power `n`, by repeated squaring to about twice the type's
 precision, so that its error does not grow past half a unit in the last
 place of `Number`.
*/
template <number Number>
twofold<Number> twofold_power(Number base, std::size_t n) {
    twofold<Number> result{Number(1), Number(0)};
    twofold<Number> square{base, Number(0)};
    while (n != 0) {
        if (n % 2 != 0) {
            result = twofold_product(result, square);
        }
        n /= 2;
        if (n != 0) {
            square = twofold_product(square, square);
        }
    }
    return result;
}

/**
 The value of a number literal, read one digit at a time.

 Tip: the first `digits10 + 1` significant digits make an integer mantissa,
 one digit more than the type holds exactly, or `max_digits10` digits where
 that is more, as in a binary type, so the mantissa rounds only near its
 end, and never overflows. A later digit of the integer part only scales
 the value up by ten, and a later digit of the fraction is below the type's
 precision, and is dropped. A type without `std::numeric_limits` keeps every
 digit. The multiplication by ten and the addition of a digit are two
 statements, so that no compiler contracts them into one fused
 multiply-add, which would round differently from one target to another.
*/
template <number Number>
class literal_value {
public:
    /** Reads `digit`, the next digit of the integer part. */
    void integer_digit(int digit) {
        if (kept_ == 0 && digit == 0) {
            return;
        }
        if (kept_ == kept_digits) {
            ++scale_up_;
            return;
        }
        keep(digit);
    }

    /** Reads `digit`, the next digit of the fraction. */
    void fraction_digit(int digit) {
        if (kept_ == kept_digits) {
            return;
        }
        ++scale_down_;
        if (kept_ == 0 && digit == 0) {
            return;
        }
        keep(digit);
    }

    /**
     The literal's value: the mantissa scaled up, or divided by ten to the
     number of its fraction digits.

     Tip: where that power of ten is infinite but the value is not, a
     subnormal, the division is made in steps of the largest finite power.
     In a binary type, the value is scaled up in two steps, by five to the
     power and then by two to it, as @ref scaled_up_in_binary says.
    */
    [[nodiscard]] Number value() const {
        if (scale_down_ == 0) {
            if constexpr (is_binary) {
                if (scale_up_ != 0 && scale_up_ <= largest_power) {
                    return scaled_up_in_binary();
                }
            }
            return Number(mantissa_ * power_of(Number(10), scale_up_));
        }
        Number value = mantissa_;
        std::size_t remaining = scale_down_;
        if (remaining > largest_power) {
            const auto step = power_of(Number(10), largest_power);
            while (remaining > largest_power && value != Number(0)) {
                value = value / step;
                remaining -= largest_power;
            }
        }
        return Number(value / power_of(Number(10), remaining));
    }

private:
    using limits = std::numeric_limits<Number>;

    /**
     How many significant digits the mantissa keeps: `digits10 + 1`, or
     `max_digits10` where that is more, as in a binary type.
    */
    static constexpr std::size_t kept_digits =
        limits::is_specialized && limits::digits10 >= 0
            ? std::max(static_cast<std::size_t>(limits::digits10) + 1,
                       static_cast<std::size_t>(limits::max_digits10))
            : std::numeric_limits<std::size_t>::max();

    /** The largest power of ten that is finite in `Number`, at least 1. */
    static constexpr std::size_t largest_power =
        limits::is_specialized && limits::max_exponent10 > 0
            ? static_cast<std::size_t>(limits::max_exponent10)
            : std::numeric_limits<std::size_t>::max();

    /** Whether `Number` is a binary floating-point type, as `double` is. */
    static constexpr bool is_binary =
        limits::is_specialized && !limits::is_integer && limits::radix == 2;

    /**
     The mantissa times ten to `scale_up_`, in a binary type: times five to
     that power, held to twice the type's precision, then times two to it,
     which is exact.

     Tip: ten to a large power by repeated squaring in `Number` is a few
     units in the last place away, enough that the largest `double`
     written out in full would overflow; here only the multiplication by
     five to the power rounds, once, and the value overflows only when the
     product rounded at the type's precision is past the range.
     @pre `0 < scale_up_ <= largest_power`.
    */
    [[nodiscard]] Number scaled_up_in_binary() const {
        const twofold<Number> power = twofold_power(Number(5), scale_up_);
        const twofold<Number> product = exact_product(mantissa_, power.high);
        Number low = mantissa_ * power.low;
        low = low + product.low;
        const Number scaled = product.high + low;
        return Number(scaled * power_of(Number(2), scale_up_));
    }

    void keep(int digit) {
        mantissa_ = mantissa_ * Number(10);
        mantissa_ = mantissa_ + Number(digit);
        ++kept_;
    }

    Number mantissa_{0};
    std::size_t kept_ = 0;
    std::size_t scale_up_ = 0;
    std::size_t scale_down_ = 0;
};

}  // namespace detail

/**
 The calculator's lexer, a lazy @ref webcpp::pratt::token_stream.

 It holds the source, its position and one token of lookahead, and lexes the
 next token only when @ref advance consumes the current one.

 A number is digits, then optionally `.` and more digits; it is computed in
 `Number` arithmetic alone, never through `double`, so `0.1` is exactly one
 tenth in a decimal type. A name is a letter or `_`, then letters, digits
 and `_`. Space, tab, carriage return and line feed separate tokens. Any
 other character is a token of kind @ref kind::invalid, one character long,
 which the grammar rejects where it stands.

 A number keeps its first `digits10 + 1` significant digits, or
 `max_digits10` where that is more, 17 in both `decimal64_t` and `double`,
 as one integer, divided once by ten to the number of its fraction digits.
 A later digit of the integer part multiplies the value by ten, and a later
 digit of the fraction is dropped. So a literal of at most `digits10`
 significant digits, with at most 22 fraction digits in `double`, is the
 nearest value; a longer one is within one unit in the last place of it in
 `decimal64_t`, and may be a few units away in `double`, whose digits
 round as they are read. `1.` followed by any number of zeros is exactly
 1; a literal up to the type's largest value is finite, the largest
 `double` written out in full included, and `1` followed by 400 zeros is
 infinite; and a fraction keeps its value down to the type's smallest
 subnormal, and is 0 below it. A type without `std::numeric_limits` keeps
 every digit.

 A non-ASCII character is a token of kind @ref kind::invalid per byte, so
 the parse stops at its first byte with @ref error::invalid_character.

 @tparam Number The type a number token's value is.
 @see "The calculator", in the guide.
*/
template <number Number>
class stream {
public:
    /** The tokens the stream yields. */
    using token_type = token<Number>;

    /**
     Constructs the stream of `source`, and lexes the first token.

     @param source The text to lex.
     @pre `source` outlives the stream and its tokens, whose text views it.
    */
    explicit stream(std::string_view source) : source_(source), next_(lex()) {}

    /**
     The next token, not consumed; the end token once the input is spent.

     @return The token, valid until the next @ref advance.
    */
    [[nodiscard]] const token_type& peek() const { return next_; }

    /**
     Consumes the next token and returns it; at the end, returns the end token again.

     @return The token consumed.
    */
    token_type advance() {
        token_type current = next_;
        if (current.type == kind::end) {
            return current;
        }
        next_ = lex();
        return current;
    }

    /**
     Whether the input is spent: the next token is the end token.

     @return `true` once only the end token remains.
    */
    [[nodiscard]] bool at_end() const { return next_.type == kind::end; }

private:
    /** Lexes the token at the position, after the whitespace before it, and moves past it. */
    token_type lex() {
        skip_whitespace();
        if (pos_ >= source_.size()) {
            return token_type{.type = kind::end, .value = Number(0), .text = source_.substr(pos_)};
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

    /** Lexes a number at the position, which is a digit. */
    token_type lex_number() {
        const std::size_t start = pos_;
        detail::literal_value<Number> literal;
        for (; pos_ < source_.size() && detail::is_digit(source_[pos_]); ++pos_) {
            literal.integer_digit(source_[pos_] - '0');
        }
        const bool has_fraction = pos_ + 1 < source_.size() && source_[pos_] == '.' &&
                                  detail::is_digit(source_[pos_ + 1]);
        if (has_fraction) {
            ++pos_;
            for (; pos_ < source_.size() && detail::is_digit(source_[pos_]); ++pos_) {
                literal.fraction_digit(source_[pos_] - '0');
            }
        }
        return number_token(start, literal.value());
    }

    /** The number token of `value`, whose text runs from `start` to the position. */
    [[nodiscard]] token_type number_token(std::size_t start, const Number& value) const {
        return token_type{
            .type = kind::number,
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
            .type = kind::name,
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
                return token_type{.type = candidate.type, .value = Number(0), .text = pair};
            }
        }
        const std::string_view one = source_.substr(pos_, 1);
        ++pos_;
        return token_type{
            .type = detail::one_character_kind(one.front()),
            .value = Number(0),
            .text = one,
        };
    }

    std::string_view source_;
    std::size_t pos_ = 0;
    token_type next_;
};

/**
 A node of a calculator parse: a value, and the name of a bare variable.

 A bare name keeps its name, so that `=`, `++` and `--` can write it, and a
 grouping keeps it too, so `(a) = 1` writes `a`. The name has a value only
 once the environment holds one: `a = 5` can define `a`, and an undefined
 name is an error only where it is read.

 @tparam Number The type the calculator computes in.
 @see "The calculator", in the guide.
*/
template <number Number>
struct node {
    /** The value, when @ref has_value says there is one. */
    Number value{0};

    /** Whether the node has a value: false for a name the environment does not hold. */
    bool has_value = true;

    /** The name of a bare variable, a view into the source; empty for every other node. */
    std::string_view name{};
};

/**
 An environment the calculator reads and writes its variables in.

 `lookup(name)` returns a pointer to the value of `name`, or `nullptr` when
 the name is not set. `assign(name, value)` sets it, and may fail, a fixed
 storage that is full for instance, with an error of the environment's own,
 which the calculator returns unchanged. The name is a view into the source,
 so an environment that keeps it copies it.

 @tparam E The environment's type, whose `number_type` models @ref number.
 @see "The calculator", in the guide.
 @see "Allocation", in the guide.
*/
template <class E>
concept environment_for = requires(E e, const E ce, std::string_view n, E::number_type v) {
    requires number<typename E::number_type>;
    { ce.lookup(n) } -> std::same_as<const typename E::number_type*>;
    { e.assign(n, v) } -> std::same_as<result<void>>;
};

/**
 The calculator's own environment: a `std::map` from names to values,
 whose map and keys allocate through the user's allocator.

 It allocates through `Allocator` alone: in @ref assign, when a name is new,
 the map's node, and the key's characters when the name is longer than the
 string's small buffer; and when it is constructed, with a standard library
 whose map allocates then, as MSVC's does. Assigning a name it holds replaces
 the value in place, and @ref lookup allocates nothing. For a `Number` that
 allocates nothing, such as `decimal64_t` or `double`, nothing else that @ref
 evaluate does allocates, so this environment's allocator is the calculator's
 only source of memory. A `Number` whose arithmetic allocates,
 Boost.Multiprecision's `mpfr_float` with `et_off` for instance, allocates
 through its own allocator.

 The key holds a `std::basic_string` whose allocator is `Allocator` rebound
 to `char`, and the map's allocator is `Allocator` rebound to the map's value
 type, so any allocator serves, whatever type it is written for:
 `environment<Number, A<char>>` and the default are both well formed. The
 map compares with `std::less<>`, so a name is looked up as a
 `std::string_view`, without building a key.

 @tparam Number The type of the values, `boost::decimal::decimal64_t` by default.
 @tparam Allocator The allocator of the map and of its keys. The default is
 `std::allocator<std::pair<const std::string, Number>>`, the map's own value
 type: `std::allocator` rebinds to `std::allocator<char>`, whose key is
 `std::string`, so the default names its own map, and depends only on
 `Number`.
 @see "Allocation", in the guide.
*/
template <number Number = boost::decimal::decimal64_t,
          class Allocator = std::allocator<std::pair<const std::string, Number>>>
class environment {
    using key_allocator = std::allocator_traits<Allocator>::template rebind_alloc<char>;

    /**
     A name the environment holds: its characters, allocated through `key_allocator`.

     Tip: a class with constructors of its own, not a bare `std::basic_string`. libstdc++'s
     `std::pair` asks whether its members are implicitly default-constructible, and for a string
     clang then instantiates the default argument `_Alloc()` of its `initializer_list`
     constructor, a hard error for an allocator without a default constructor, which the
     Allocator requirements allow. Asked of this class, the question has a plain answer: no.
    */
    class key {
    public:
        key(std::string_view name, const key_allocator& allocator) : text_(name, allocator) {}

        friend std::strong_ordering operator<=>(const key& a, const key& b) noexcept = default;

        friend bool operator==(const key& a, std::string_view b) noexcept {
            return std::string_view(a.text_) == b;
        }

        friend std::strong_ordering operator<=>(const key& a, std::string_view b) noexcept {
            return std::string_view(a.text_) <=> b;
        }

    private:
        std::basic_string<char, std::char_traits<char>, key_allocator> text_;
    };

    using map_allocator =
        std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const key, Number>>;

public:
    /** The type of the values. */
    using number_type = Number;

    /** The allocator the environment was given, which its map and its keys rebind. */
    using allocator_type = Allocator;

    /** Constructs an empty environment with a default-constructed allocator. */
    environment() = default;

    /**
     Constructs an empty environment that allocates through `allocator`.

     @param allocator The allocator, rebound for the map and for its keys.
    */
    explicit environment(const Allocator& allocator) : names_(map_allocator(allocator)) {}

    /**
     The allocator the environment allocates through.

     @return A copy of it, rebound back to `Allocator`, as an allocator-aware
     standard container returns its own.
    */
    [[nodiscard]] allocator_type get_allocator() const noexcept {
        return allocator_type(names_.get_allocator());
    }

    /**
     Finds the value of `name`.

     @param name The variable's name.
     @return A pointer to its value, valid until the name is assigned again
     or the environment is destroyed, or `nullptr` when the name is not set.
    */
    [[nodiscard]] const Number* lookup(std::string_view name) const {
        const auto found = names_.find(name);
        if (found == names_.end()) {
            return nullptr;
        }
        return &found->second;
    }

    /**
     Sets `name` to `value`, inserting the name or replacing its value.

     @param name The variable's name, which the environment copies.
     @param value Its new value.
     @return Success. A new name allocates its node and its key through the
     allocator; a failure to allocate is the allocator's, as in any
     standard container.
    */
    result<void> assign(std::string_view name, Number value) {
        const auto at = names_.lower_bound(name);
        if (at != names_.end() && at->first == name) {
            at->second = std::move(value);
            return {};
        }
        names_.emplace_hint(at, key(name, key_allocator(names_.get_allocator())), std::move(value));
        return {};
    }

private:
    std::map<key, Number, std::less<>, map_allocator> names_;
};

namespace detail {

/** `a / b`, or @ref error::division_by_zero when `b` is zero. */
template <number Number>
result<Number> divide(const Number& a, const Number& b) {
    if (b == Number(0)) {
        // In place, as every error into a result of Number: a Number may convert from an error,
        // as Boost.Multiprecision's numbers convert from any enum.
        return {boost::system::in_place_error, error::division_by_zero};
    }
    return Number(a / b);
}

/**
 The remainder of `a / b`, with the sign of `a`, or
 @ref error::division_by_zero when `b` is zero.
*/
template <number Number>
result<Number> remainder(const Number& a, const Number& b) {
    if (b == Number(0)) {
        return {boost::system::in_place_error, error::division_by_zero};
    }
    return math::adl_fmod(a, b);
}

/**
 Whether `x` is an integer: equal to its truncation, and finite.

 Tip: an infinity equals its truncation too, but halving it never reaches
 zero; `x * 0` is zero for a finite `x` and NaN for an infinity.
*/
template <number Number>
bool is_integral(const Number& x) {
    const Number zero(0);
    return math::adl_trunc(x) == x && Number(x * zero) == zero;
}

/**
 Whether `x`, a finite integer, is odd.

 Tip: an integer whose successor rounds back to it is past the type's
 precision, so a multiple of its radix, 2 or 10, and even. Below that,
 `fmod(x, 2)` is exact; past it, Boost.Decimal's is not:
 `fmod(9999999999999999e368, 2)` gives -1e368.
*/
template <number Number>
bool is_odd(const Number& x) {
    if (Number(x + Number(1)) == x) {
        return false;
    }
    return math::adl_fmod(x, Number(2)) != Number(0);
}

/**
 `base` to the power `exponent`, a finite integer at least 0, by repeated
 squaring in `Number`.

 Tip: each step halves the exponent with `trunc`, so a finite exponent ends
 within as many steps as its binary digits. Every product is exact while it
 fits the type's precision, so the result is exact whenever it does. Past
 the precision a half may round, which moves the exponent by a few units in
 its last place, and the result by as much relatively.
*/
template <number Number>
Number power_by_squaring(Number base, Number exponent) {
    const Number zero(0);
    const Number two(2);
    Number result(1);
    while (exponent != zero) {
        const Number half = math::adl_trunc(Number(exponent / two));
        if (exponent != half * two) {
            result = result * base;
        }
        exponent = half;
        if (exponent != zero) {
            base = base * base;
        }
    }
    return result;
}

/**
 `base ^ exponent`: by repeated squaring for an integral exponent, through
 `pow` for any other.

 The square is of `|base|`, and the sign is the base's when the exponent is
 odd, so a halving that rounds never changes it. A negative integral
 exponent is `1 / |base|^|n|`; when that power is 0 or not finite, past the
 type's range, `pow` computes the result instead, which is then a subnormal,
 0 or infinite. `0 ^ -n` gives @ref error::division_by_zero, as `1 / 0`
 does, and `x ^ 0` is 1 for every `x`, `0 ^ 0` included.
*/
template <number Number>
result<Number> power(const Number& base, const Number& exponent) {
    if (!is_integral(exponent)) {
        return math::adl_pow(base, exponent);
    }
    const Number zero(0);
    if (exponent < zero && base == zero) {
        return {boost::system::in_place_error, error::division_by_zero};
    }
    const bool negative = base < zero && is_odd(exponent);
    const Number magnitude = base < zero ? Number(-base) : base;
    Number value = power_by_squaring(magnitude, exponent < zero ? Number(-exponent) : exponent);
    if (exponent < zero) {
        const bool out_of_range = value == zero || Number(value * zero) != zero;
        value = out_of_range ? math::adl_pow(magnitude, exponent) : Number(Number(1) / value);
    }
    return negative ? Number(-value) : value;
}

}  // namespace detail

/**
 The calculator's grammar over an environment: numbers, variables, the
 arithmetic operators `+ - * / % ^`, the comparisons, `=`, and `++` and
 `--` before and after a variable.

 `^` is right-associative and binds tighter than a unary sign, so
 `-2 ^ 2 == -4`; `=` is right-associative, so `c = d = 5` sets both. A
 comparison gives 1 or 0. `++x` gives the new value and `x++` the old, as
 in C. An integral exponent is computed by repeated squaring in `Number`,
 so an integer power is exact while it fits the type's precision; past
 that, the relative error grows with the exponent, where a correctly
 rounded `pow` would not, which is the price of the same bits on every
 platform. `x ^ -n` is `1 / x ^ n`, or `pow`'s result when `x ^ n`
 overflows or underflows to 0; otherwise only a non-integral exponent
 calls `pow`.

 The tokens are read one at a time, and the first error found in reading
 order is the one returned. A token's error is found when the token is
 read, and an operator's when the operator has its operands: `=` checks its
 target before it reads its right operand, so `5 = 3 $` gives
 @ref error::invalid_assignment_target, while `+` finds an undefined
 operand, or `/` a zero divisor, only after its right operand is read, so
 `x + 1 $` gives @ref error::invalid_character even when `x` is undefined,
 and so does `1 / 0 $`.

 A write made before the error stays made: nothing is undone. `i++ $`
 increments `i`, then gives @ref error::invalid_character, and `i++ )`
 increments it, then gives @ref error::trailing_input.

 @tparam Environment The environment the grammar reads and writes, which
 must outlive the grammar.
 @see "The calculator", in the guide.
*/
template <environment_for Environment>
class grammar {
public:
    /** The type the calculator computes in, the environment's. */
    using number_type = Environment::number_type;

    /** The tokens the grammar reads. */
    using token_type = token<number_type>;

    /** What a parse builds. */
    using node_type = node<number_type>;

    /** The power of `=`, the weakest operator. */
    static constexpr int assign_bp = 10;

    /** The power of the six comparisons. */
    static constexpr int compare_bp = 20;

    /** The power of binary `+` and `-`. */
    static constexpr int additive_bp = 30;

    /** The power of `*`, `/` and `%`. */
    static constexpr int multiplicative_bp = 40;

    /** The power a unary sign and a prefix `++` or `--` parse their operand with, below `^`. */
    static constexpr int unary_bp = 45;

    /** The power of `^`. */
    static constexpr int power_bp = 50;

    /** The power of a postfix `++` or `--`, the tightest operator. */
    static constexpr int postfix_bp = 70;

    /**
     The power of an invalid character, higher than every other, so that one
     after a complete operand is read as an operator, whose led gives
     @ref error::invalid_character, rather than end the expression with
     @ref error::trailing_input or @ref error::unexpected_end.

     An error found earlier in reading order is still returned first, and a
     write made before the character stays made.
    */
    static constexpr int invalid_bp = std::numeric_limits<int>::max();

    /**
     Constructs the grammar over `env`.

     @param env The environment.
     @pre `env` outlives the grammar.
    */
    explicit grammar(Environment& env) : env_(&env) {}

    /**
     How tightly the token `t` binds the expression on its left.

     @param t The token.
     @return Its power, as the constants above give it; 0 for a token that
     takes no left operand.
    */
    [[nodiscard]] int lbp(const token_type& t) const {
        switch (t.type) {
            case kind::assign: return assign_bp;
            case kind::eq:
            case kind::ne:
            case kind::lt:
            case kind::le:
            case kind::gt:
            case kind::ge: return compare_bp;
            case kind::plus:
            case kind::minus: return additive_bp;
            case kind::star:
            case kind::slash:
            case kind::percent: return multiplicative_bp;
            case kind::caret: return power_bp;
            case kind::increment:
            case kind::decrement: return postfix_bp;
            case kind::invalid: return invalid_bp;
            default: return 0;
        }
    }

    // nud() and led() recurse through the engine's expression(): that is Pratt parsing, bounded by
    // the parser's max_depth, hence the exemption from misc-no-recursion.

    /**
     The meaning of the token `t` at the start of an expression.

     @tparam Parser The parser's type, a @ref webcpp::pratt::parser of this grammar.
     @param p The parser.
     @param t The token.
     @return The node: a number, a variable, a grouping, a unary sign, or a
     prefix `++` or `--`; or the error of the token or of its operand.
    */
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    result<node_type> nud(Parser& p, const token_type& t) const {
        switch (t.type) {
            case kind::number:
                return literal([](const token_type& n) { return value_node(n.value); }, t);
            case kind::name: return read(t.text);
            case kind::lparen:
                return grouping(p, [](const token_type& n) { return n.type == kind::rparen; });
            case kind::plus:
                return prefix(p, unary_bp, [](const node_type& operand) -> result<node_type> {
                    if (!operand.has_value) {
                        return error::undefined_variable;
                    }
                    return value_node(operand.value);
                });
            case kind::minus:
                return prefix(p, unary_bp, [](const node_type& operand) -> result<node_type> {
                    if (!operand.has_value) {
                        return error::undefined_variable;
                    }
                    return value_node(number_type(-operand.value));
                });
            case kind::increment:
            case kind::decrement: {
                const number_type step(t.type == kind::increment ? 1 : -1);
                return prefix(p, unary_bp, [this, &step](const node_type& target) {
                    return step_before(target, step);
                });
            }
            case kind::invalid: return error::invalid_character;
            case kind::end: return error::unexpected_end;
            default: return error::unexpected_token;
        }
    }

    /**
     The meaning of the token `t` after the expression `left`.

     @tparam Parser The parser's type, a @ref webcpp::pratt::parser of this grammar.
     @param p The parser.
     @param t The token.
     @param left The expression on its left.
     @return The node of the binary operator, of the assignment or of the
     postfix `++` or `--`; or the error of the token or of its operands.
    */
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    result<node_type> led(Parser& p, const token_type& t, const node_type& left) const {
        switch (t.type) {
            case kind::assign:
                if (left.name.empty()) {
                    return error::invalid_assignment_target;
                }
                return infix_right(p, assign_bp, left,
                                   [this](const node_type& target, const node_type& value) {
                                       return write(target, value);
                                   });
            case kind::plus: return infix_left(p, additive_bp, left, arithmetic(add));
            case kind::minus: return infix_left(p, additive_bp, left, arithmetic(subtract));
            case kind::star: return infix_left(p, multiplicative_bp, left, arithmetic(multiply));
            case kind::slash:
                return infix_left(p, multiplicative_bp, left,
                                  arithmetic(detail::divide<number_type>));
            case kind::percent:
                return infix_left(p, multiplicative_bp, left,
                                  arithmetic(detail::remainder<number_type>));
            case kind::caret:
                return infix_right(p, power_bp, left, arithmetic(detail::power<number_type>));
            case kind::eq:
            case kind::ne:
            case kind::lt:
            case kind::le:
            case kind::gt:
            case kind::ge:
                return infix_left(
                    p, compare_bp, left,
                    arithmetic([op = t.type](const number_type& a, const number_type& b) {
                        return compare(op, a, b);
                    }));
            case kind::increment:
            case kind::decrement: {
                const number_type step(t.type == kind::increment ? 1 : -1);
                return postfix(left, [this, &step](const node_type& target) {
                    return step_after(target, step);
                });
            }
            case kind::invalid: return error::invalid_character;
            default: return error::unexpected_token;
        }
    }

private:
    static result<node_type> value_node(const number_type& value) {
        return node_type{.value = value, .has_value = true, .name = {}};
    }

    static result<number_type> add(const number_type& a, const number_type& b) {
        return number_type(a + b);
    }

    static result<number_type> subtract(const number_type& a, const number_type& b) {
        return number_type(a - b);
    }

    static result<number_type> multiply(const number_type& a, const number_type& b) {
        return number_type(a * b);
    }

    /** The comparison `op` of `a` and `b`, as 1 or 0. */
    static result<number_type> compare(kind op, const number_type& a, const number_type& b) {
        bool holds = false;
        switch (op) {
            case kind::eq: holds = a == b; break;
            case kind::ne: holds = a != b; break;
            case kind::lt: holds = a < b; break;
            case kind::le: holds = a <= b; break;
            case kind::gt: holds = a > b; break;
            default: holds = a >= b; break;
        }
        return number_type(holds ? 1 : 0);
    }

    /** The make of a binary operator: both operands read as values, then `op` of the two. */
    template <class Op>
    static auto arithmetic(Op op) {
        return [op](const node_type& l, const node_type& r) -> result<node_type> {
            if (!l.has_value || !r.has_value) {
                return error::undefined_variable;
            }
            const result<number_type> value = op(l.value, r.value);
            if (value.has_error()) {
                return value.error();
            }
            return value_node(*value);
        };
    }

    /** The node of the name `name`: its value when the environment holds one, and the name. */
    [[nodiscard]] result<node_type> read(std::string_view name) const {
        const number_type* found = env_->lookup(name);
        if (found == nullptr) {
            return node_type{.value = number_type(0), .has_value = false, .name = name};
        }
        return node_type{.value = *found, .has_value = true, .name = name};
    }

    /** Writes `value` into the variable `target`, and gives the value. */
    [[nodiscard]] result<node_type> write(const node_type& target, const node_type& value) const {
        if (!value.has_value) {
            return error::undefined_variable;
        }
        const result<void> written = env_->assign(target.name, value.value);
        if (written.has_error()) {
            return written.error();
        }
        return value_node(value.value);
    }

    /** Adds `step` to the variable `target`, and gives the new value: `++x`, `--x`. */
    [[nodiscard]] result<node_type> step_before(const node_type& target,
                                                const number_type& step) const {
        if (target.name.empty()) {
            return error::invalid_assignment_target;
        }
        if (!target.has_value) {
            return error::undefined_variable;
        }
        const number_type updated(target.value + step);
        const result<void> written = env_->assign(target.name, updated);
        if (written.has_error()) {
            return written.error();
        }
        return value_node(updated);
    }

    /** Adds `step` to the variable `target`, and gives the old value: `x++`, `x--`. */
    [[nodiscard]] result<node_type> step_after(const node_type& target,
                                               const number_type& step) const {
        if (target.name.empty()) {
            return error::invalid_assignment_target;
        }
        if (!target.has_value) {
            return error::undefined_variable;
        }
        const result<void> written = env_->assign(target.name, number_type(target.value + step));
        if (written.has_error()) {
            return written.error();
        }
        return value_node(target.value);
    }

    Environment* env_;
};

/**
 Evaluates `source` as one expression of the calculator, reading and
 writing its variables in `env`.

 For a `Number` that allocates nothing, such as `decimal64_t` or `double`,
 nothing is allocated but what `env` allocates to store a name: the lexer,
 the tokens and the nodes are views and numbers. A `Number` whose
 arithmetic allocates, Boost.Multiprecision's `mpfr_float` with `et_off`
 for instance, allocates through its own allocator.

 The first error found in reading order is the one returned, and the writes
 made before it stay made, as @ref grammar says: a failed evaluation may have
 assigned variables in `env`. A caller that needs all or nothing evaluates
 in a copy of the environment and keeps the copy on success.

 @tparam Environment The environment's type, which models
 @ref environment_for.
 @param source The expression, which needs to live only for the call.
 @param env The environment.
 @param max_depth The deepest nesting of the engine's expression calls
 allowed, as @ref webcpp::pratt::parser takes it: a negative value behaves
 as 0.
 @return The expression's value; the first error of the parse, an error of
 @ref error, or the error `env.assign` returned; or
 @ref error::undefined_variable when the whole expression is a name the
 environment does not hold.
 @see "The calculator", in the guide.
 @see "Errors", in the guide.
*/
template <environment_for Environment>
result<typename Environment::number_type> evaluate(std::string_view source, Environment& env,
                                                   int max_depth = default_max_depth) {
    using number_type = Environment::number_type;
    const grammar<Environment> rules(env);
    const result<node<number_type>> parsed = parse(rules, stream<number_type>(source), max_depth);
    if (parsed.has_error()) {
        return {boost::system::in_place_error, parsed.error()};
    }
    if (!parsed->has_value) {
        return {boost::system::in_place_error, error::undefined_variable};
    }
    return parsed->value;
}

}  // namespace webcpp::pratt::calc

#endif  // WEBCPP_PRATT_CALCULATOR_HPP
