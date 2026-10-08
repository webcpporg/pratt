// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A grammar written with every helper of the engine, whose node is the S-expression of the parse,
// so that each line shows how its operators grouped.

#include <webcpp/pratt.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace pratt = webcpp::pratt;

namespace {

/** The characters of `text`, one token each, and '\0' once the text is spent. */
class characters {
public:
    using token_type = char;

    explicit characters(std::string_view text) : text_(text) {}

    [[nodiscard]] char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    char advance() {
        const char token = peek();
        if (pos_ < text_.size()) {
            ++pos_;
        }
        return token;
    }

    [[nodiscard]] bool at_end() const { return pos_ == text_.size(); }

private:
    std::string_view text_;
    std::size_t pos_ = 0;
};

/** The node `(<op> left right)` of a binary operator, as the infix helpers call it. */
auto binary(char op) {
    return [op](std::string left, std::string right) -> pratt::result<std::string> {
        return "(" + std::string(1, op) + " " + std::move(left) + " " + std::move(right) + ")";
    };
}

/** The node `(<op> operand)` of a unary operator, as prefix and postfix call it. */
auto unary(char op) {
    return [op](std::string operand) -> pratt::result<std::string> {
        return "(" + std::string(1, op) + " " + std::move(operand) + ")";
    };
}

// tag::powers[]
/** Digits, `+ - * / ^`, a unary minus, a postfix `!`, and parentheses, as S-expressions. */
struct sexpr {
    using node_type = std::string;

    static constexpr int additive = 10;
    static constexpr int multiplicative = 20;
    static constexpr int sign = 30;
    static constexpr int power = 40;
    static constexpr int factorial = 50;

    [[nodiscard]] static int lbp(char token) {
        switch (token) {
            case '+':
            case '-': return additive;
            case '*':
            case '/': return multiplicative;
            case '^': return power;
            case '!': return factorial;
            default: return 0;
        }
    }

    // end::powers[]

    // nud() and led() recurse through the helpers and the parser's expression(), which max_depth
    // bounds, hence the exemption from misc-no-recursion, outside the regions the guide shows.
    // NOLINTBEGIN(misc-no-recursion)

    // tag::nud[]
    template <class Parser>
    pratt::result<std::string> nud(Parser& p, char token) const {
        if (token >= '0' && token <= '9') {
            return pratt::literal(
                [](char digit) -> pratt::result<std::string> { return std::string(1, digit); },
                token);
        }
        switch (token) {
            case '-': return pratt::prefix(p, sign, unary('-'));
            case '(': return pratt::grouping(p, [](char next) { return next == ')'; });
            case '\0': return pratt::error::unexpected_end;
            default: return pratt::error::unexpected_token;
        }
    }

    // end::nud[]

    // tag::led[]
    template <class Parser>
    pratt::result<std::string> led(Parser& p, char token, std::string left) const {
        switch (token) {
            case '+':
            case '-':
            case '*':
            case '/': return pratt::infix_left(p, lbp(token), std::move(left), binary(token));
            case '^': return pratt::infix_right(p, power, std::move(left), binary('^'));
            default: return pratt::postfix(std::move(left), unary('!'));
        }
    }

    // end::led[]
    // NOLINTEND(misc-no-recursion)
    // tag::close[]
};

// end::close[]

}  // namespace

// tag::main[]
int main() {
    const sexpr grammar;
    for (const std::string_view text :
         {"1-2-3", "2^3^2", "1+2*3", "-2^2", "-3!", "2*-3", "(1+2)*3", "(1+2", "1+)", "()"}) {
        const pratt::result<std::string> tree = pratt::parse(grammar, characters(text));
        if (tree.has_error()) {
            std::cout << text << " -> error: " << tree.error().message() << '\n';
        } else {
            std::cout << text << " -> " << *tree << '\n';
        }
    }
}

// end::main[]
