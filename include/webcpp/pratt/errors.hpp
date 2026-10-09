// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The errors a Pratt parse reports, as `boost::system::error_code` values of a
 category of its own, and the result type of every operation that can fail.

 @see "Errors", in the guide.
*/
#ifndef WEBCPP_PRATT_ERRORS_HPP
#define WEBCPP_PRATT_ERRORS_HPP

#include <webcpp/pratt/config.hpp>

#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace webcpp::pratt {

/**
 The errors pratt reports, as `boost::system::error_code` values of the
 category @ref error_category.

 The values start at 1, so an `error_code` of value 0 is no error. An
 `error` converts to an `error_code` and compares with one:
 `parsed.error() == pratt::error::trailing_input`.

 @note The values are fixed: they are never reused for a different meaning,
 so a stored value keeps its meaning across versions.
 @see "Errors", in the guide.
*/
enum class error : int {
    /** A token appears where the grammar's `nud` or `led` has none for it. */
    unexpected_token = 1,

    /** The input ends where a token was still expected. */
    unexpected_end = 2,

    /** A closing token the grammar requires is missing. */
    expected_closing_token = 3,

    /** Input remains after a complete expression. */
    trailing_input = 4,

    /** The left side of an assignment or an increment is not a name. */
    invalid_assignment_target = 5,

    /** A name is read before anything has written it. */
    undefined_variable = 6,

    /** A character the lexer reads as no token, such as `$`, or `!` without `=`. */
    invalid_character = 7,

    /** A parse recursed past the parser's configured depth limit. */
    nesting_too_deep = 8,

    /** A division or a remainder was asked by zero. */
    division_by_zero = 9,
};

namespace detail {

/**
 The human message of an @ref error value, or the unknown message for a
 value it does not define.
*/
inline const char* message_of(int value) noexcept {
    switch (static_cast<error>(value)) {
        case error::unexpected_token: return "unexpected token";
        case error::unexpected_end: return "unexpected end of input";
        case error::expected_closing_token: return "expected closing token";
        case error::trailing_input: return "trailing input after expression";
        case error::invalid_assignment_target: return "invalid assignment target";
        case error::undefined_variable: return "undefined variable";
        case error::invalid_character: return "invalid character";
        case error::nesting_too_deep: return "expression nested too deeply";
        case error::division_by_zero: return "division by zero";
    }
    return "unknown webcpp.pratt error";
}

/**
 A constant category with a fixed identity, so it holds no mutable state.

 @note The identity is the ASCII of "pratt" followed by three zero bytes.
*/
class category final : public boost::system::error_category {
public:
    /** Constructs the category with its fixed identity. */
    constexpr category() noexcept : boost::system::error_category(0x7072617474000000ULL) {}

    /** The category's name, "webcpp.pratt". */
    const char* name() const noexcept override { return "webcpp.pratt"; }

    /** The human message of the error `value`, as @ref message_of spells it. */
    std::string message(int value) const override { return message_of(value); }

    /**
     Copies the human message of the error `value` into `buffer`, cut to fit `size` bytes with
     its terminating zero, and returns the whole message.
    */
    const char* message(int value, char* buffer, std::size_t size) const noexcept override {
        const char* text = message_of(value);
        if (buffer != nullptr && size != 0) {
            const std::string_view spelling(text);
            const std::size_t limit = size - 1;
            const std::size_t count = spelling.size() < limit ? spelling.size() : limit;
            std::memcpy(buffer, spelling.data(), count);
            buffer[count] = '\0';
        }
        return text;
    }
};

/** The one category of pratt's errors, which @ref error_category returns. */
inline constexpr category the_category{};

}  // namespace detail

/**
 The category of every @ref error.

 Its `name()` is "webcpp.pratt", and its `message(value)` the human message
 of the error, "division by zero" for @ref error::division_by_zero, or
 "unknown webcpp.pratt error" for a value @ref error does not define.

 @return The category, a constant with a fixed identity.
 @see "Errors", in the guide.
*/
inline const boost::system::error_category& error_category() noexcept {
    return detail::the_category;
}

/**
 Makes the `error_code` of an @ref error, as its implicit conversion does.

 @param e The error.
 @return `e` as an `error_code` of the category @ref error_category.
*/
inline boost::system::error_code make_error_code(error e) noexcept {
    return {static_cast<int>(e), error_category()};
}

/**
 What a parse that can fail returns: a `T`, or the `error_code` of the
 failure.

 It is Boost.System's `result`. pratt throws nothing of its own: every
 operation of it that can fail returns one, and so does every nud and led of
 a grammar. Its error is an @ref error, or an error of any category that a
 grammar or an environment of the user's returns, which pratt passes on
 unchanged.

 @tparam T The type of the value; `void` for an operation that returns none.
 @note When `T` converts from an @ref error, as a Boost.Multiprecision
 number does, `result` takes neither implicitly: build the error in place,
 `{boost::system::in_place_error, error::unexpected_token}`, as pratt does.
 @see "Errors", in the guide.
*/
template <class T>
using result = boost::system::result<T>;

}  // namespace webcpp::pratt

namespace boost::system {
template <>
struct is_error_code_enum<webcpp::pratt::error> : std::true_type {};
}  // namespace boost::system

#endif  // WEBCPP_PRATT_ERRORS_HPP
