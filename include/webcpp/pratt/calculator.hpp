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

/**
 A node of a calculator parse: a value, and the name of a bare variable.

 A bare name keeps its name, so that `=`, `++` and `--` can write it, and a
 grouping keeps it too, so `(a) = 1` writes `a`. The name has a value only
 once the environment holds one: `a = 5` can define `a`, and an undefined
 name is an error only where it is read.

 @tparam Number The type the calculator computes in.
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

 The key is a `std::basic_string` whose allocator is `Allocator` rebound to
 `char`, and the map's allocator is `Allocator` rebound to the map's value
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
*/
template <number Number = boost::decimal::decimal64_t,
          class Allocator = std::allocator<std::pair<const std::string, Number>>>
class environment {
    using key_allocator = std::allocator_traits<Allocator>::template rebind_alloc<char>;
    using key_type = std::basic_string<char, std::char_traits<char>, key_allocator>;
    using map_allocator =
        std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const key_type, Number>>;

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
        names_.emplace_hint(at, key_type(name, key_allocator(names_.get_allocator())),
                            std::move(value));
        return {};
    }

private:
    std::map<key_type, Number, std::less<>, map_allocator> names_;
};

namespace detail {

/** `a / b`, or @ref error::division_by_zero when `b` is zero. */
template <number Number>
result<Number> divide(const Number& a, const Number& b) {
    if (b == Number(0)) {
        return error::division_by_zero;
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
        return error::division_by_zero;
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
 `base` to the power `exponent`, a finite integer at least 0, by repeated
 squaring in `Number`.

 Tip: each step halves the exponent with `trunc`, so a finite exponent ends
 within as many steps as its binary digits. Every product is exact while it
 fits the type's precision, so the result is exact whenever it does.
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

 A negative integral exponent is `1 / base^|n|`, so `0 ^ -1` is `1 / 0`, and
 gives @ref error::division_by_zero as that division does. `x ^ 0` is 1 for
 every `x`, `0 ^ 0` included.
*/
template <number Number>
result<Number> power(const Number& base, const Number& exponent) {
    if (!is_integral(exponent)) {
        return math::adl_pow(base, exponent);
    }
    if (exponent >= Number(0)) {
        return power_by_squaring(base, exponent);
    }
    return divide(Number(1), power_by_squaring(base, Number(-exponent)));
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
 so an integer power is exact while it fits the type's precision, and a
 negative one is `1 / x^|n|`; only a non-integral exponent calls `pow`.

 @tparam Environment The environment the grammar reads and writes, which
 must outlive the grammar.
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
     The power of an invalid character, higher than every other, so that the
     engine reads it wherever it stands and reports
     @ref error::invalid_character there.
    */
    static constexpr int invalid_bp = std::numeric_limits<int>::max();

    /**
     Constructs the grammar over `env`.

     @param env The environment, which must outlive the grammar.
    */
    explicit grammar(Environment& env) : env_(&env) {}

    /**
     How tightly the token `t` binds the expression on its left.

     @param t The token.
     @return Its power, as the constants above give it; 0 for a token that
     takes no left operand.
    */
    [[nodiscard]] int lbp(const token_type& t) const {
        switch (t.k) {
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

     @param p The parser.
     @param t The token.
     @return The node: a number, a variable, a grouping, a unary sign, or a
     prefix `++` or `--`; or the error of the token or of its operand.
    */
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    result<node_type> nud(Parser& p, const token_type& t) const {
        switch (t.k) {
            case kind::number:
                return literal([](const token_type& n) { return value_node(n.value); }, t);
            case kind::name: return read(t.text);
            case kind::lparen:
                return grouping(p, [](const token_type& n) { return n.k == kind::rparen; });
            case kind::plus:
                return prefix(p, unary_bp, [](node_type operand) -> result<node_type> {
                    if (!operand.has_value) {
                        return error::undefined_variable;
                    }
                    return value_node(operand.value);
                });
            case kind::minus:
                return prefix(p, unary_bp, [](node_type operand) -> result<node_type> {
                    if (!operand.has_value) {
                        return error::undefined_variable;
                    }
                    return value_node(number_type(-operand.value));
                });
            case kind::increment:
            case kind::decrement: {
                const number_type step(t.k == kind::increment ? 1 : -1);
                return prefix(p, unary_bp,
                              [this, step](node_type target) { return step_before(target, step); });
            }
            case kind::invalid: return error::invalid_character;
            case kind::end: return error::unexpected_end;
            default: return error::unexpected_token;
        }
    }

    /**
     The meaning of the token `t` after the expression `left`.

     @param p The parser.
     @param t The token.
     @param left The expression on its left.
     @return The node of the binary operator, of the assignment or of the
     postfix `++` or `--`; or the error of the token or of its operands.
    */
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    result<node_type> led(Parser& p, const token_type& t, node_type left) const {
        switch (t.k) {
            case kind::assign:
                if (left.name.empty()) {
                    return error::invalid_assignment_target;
                }
                return infix_right(
                    p, assign_bp, std::move(left),
                    [this](node_type target, node_type value) { return write(target, value); });
            case kind::plus: return infix_left(p, additive_bp, std::move(left), arithmetic(add));
            case kind::minus:
                return infix_left(p, additive_bp, std::move(left), arithmetic(subtract));
            case kind::star:
                return infix_left(p, multiplicative_bp, std::move(left), arithmetic(multiply));
            case kind::slash:
                return infix_left(p, multiplicative_bp, std::move(left),
                                  arithmetic(detail::divide<number_type>));
            case kind::percent:
                return infix_left(p, multiplicative_bp, std::move(left),
                                  arithmetic(detail::remainder<number_type>));
            case kind::caret:
                return infix_right(p, power_bp, std::move(left),
                                   arithmetic(detail::power<number_type>));
            case kind::eq:
            case kind::ne:
            case kind::lt:
            case kind::le:
            case kind::gt:
            case kind::ge:
                return infix_left(p, compare_bp, std::move(left),
                                  arithmetic([k = t.k](const number_type& a, const number_type& b) {
                                      return compare(k, a, b);
                                  }));
            case kind::increment:
            case kind::decrement: {
                const number_type step(t.k == kind::increment ? 1 : -1);
                return postfix(std::move(left),
                               [this, step](node_type target) { return step_after(target, step); });
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

    /** The comparison `k` of `a` and `b`, as 1 or 0. */
    static result<number_type> compare(kind k, const number_type& a, const number_type& b) {
        bool holds = false;
        switch (k) {
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
        return [op](node_type l, node_type r) -> result<node_type> {
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

 Nothing is allocated but what `env` allocates to store a name: the lexer,
 the tokens and the nodes are views and numbers.

 @param source The expression, which needs to live only for the call.
 @param env The environment.
 @param max_depth The deepest nesting of the engine's expression calls
 allowed.
 @return The expression's value; the first error of the parse, an error of
 @ref error, or the error `env.assign` returned; or
 @ref error::undefined_variable when the whole expression is a name the
 environment does not hold.
*/
template <environment_for Environment>
result<typename Environment::number_type> evaluate(std::string_view source, Environment& env,
                                                   int max_depth = default_max_depth) {
    using number_type = Environment::number_type;
    const grammar<Environment> rules(env);
    const result<node<number_type>> parsed = parse(rules, stream<number_type>(source), max_depth);
    if (parsed.has_error()) {
        return parsed.error();
    }
    if (!parsed->has_value) {
        return error::undefined_variable;
    }
    return parsed->value;
}

}  // namespace webcpp::pratt::calc

#endif  // WEBCPP_PRATT_CALCULATOR_HPP
