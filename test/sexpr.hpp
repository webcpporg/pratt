// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A second grammar, with token and node types of its own: the tokens are the input's characters
// and a node is the parse written as an S-expression. It reaches the engine and its helpers
// unchanged, which is what shows the engine generic.

#ifndef WEBCPP_TEST_PRATT_SEXPR_HPP
#define WEBCPP_TEST_PRATT_SEXPR_HPP

#include <webcpp/pratt/parser.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace webcpp::test::sexpr {

/** The characters of a text, one token each; '\0' past its end. */
class stream {
public:
    using token_type = char;

    explicit stream(std::string_view source) : source_(source) {}

    [[nodiscard]] char peek() const {
        return position_ < source_.size() ? source_[position_] : '\0';
    }

    char advance() {
        const char token = peek();
        if (position_ < source_.size()) {
            ++position_;
        }
        return token;
    }

    [[nodiscard]] bool at_end() const { return position_ >= source_.size(); }

private:
    std::string_view source_;
    std::size_t position_ = 0;
};

/** Digits, `( )`, prefix `-`, left-associative `+ - *` and right-associative `^`. */
struct grammar {
    using node_type = std::string;

    [[nodiscard]] static int lbp(char token) {
        switch (token) {
            case '+':
            case '-': return 10;
            case '*': return 20;
            case '^': return 30;
            default: return 0;
        }
    }

    // Recurses through the engine's expression(): Pratt's recursion, bounded by max_depth.
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<std::string> nud(Parser& parser, char token) const {
        if (token >= '0' && token <= '9') {
            return pratt::literal(
                [](char digit) -> pratt::result<std::string> { return std::string(1, digit); },
                token);
        }
        if (token == '(') {
            return pratt::grouping(parser, [](char next) { return next == ')'; });
        }
        if (token == '-') {
            return pratt::prefix(parser, 40,
                                 [](std::string&& operand) -> pratt::result<std::string> {
                                     return "(neg " + std::move(operand) + ")";
                                 });
        }
        if (token == '\0') {
            return pratt::error::unexpected_end;
        }
        return pratt::error::unexpected_token;
    }

    // Recurses through the engine's expression(): Pratt's recursion, bounded by max_depth.
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<std::string> led(Parser& parser, char token, std::string left) const {
        const auto join = [token](std::string&& l, std::string&& r) -> pratt::result<std::string> {
            return "(" + std::string(1, token) + " " + std::move(l) + " " + std::move(r) + ")";
        };
        if (token == '^') {
            return pratt::infix_right(parser, lbp(token), std::move(left), join);
        }
        return pratt::infix_left(parser, lbp(token), std::move(left), join);
    }
};

}  // namespace webcpp::test::sexpr

#endif  // WEBCPP_TEST_PRATT_SEXPR_HPP
