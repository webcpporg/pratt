// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The guide's first grammar: integers of one digit, `+`, `-` and `*`, evaluated while they are
// parsed, with every nud and led written by hand on the parser's expression().

// tag::program[]
// tag::includes[]
#include <webcpp/pratt.hpp>

#include <cstddef>
#include <iostream>
#include <string_view>

namespace pratt = webcpp::pratt;
// end::includes[]

namespace {

// tag::stream[]
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

// end::stream[]
// tag::grammar[]
/** Integers of one digit, a unary minus, and the binary `+`, `-` and `*`. */
struct arithmetic {
    using node_type = int;

    /** How tightly `token` binds the expression on its left: 0 ends an expression. */
    [[nodiscard]] static int lbp(char token) {
        switch (token) {
            case '+':
            case '-': return 10;
            case '*': return 20;
            default: return 0;
        }
    }

    // end::grammar[]
    // nud() and led() recurse through the parser's expression(), which max_depth bounds, hence the
    // exemption from misc-no-recursion, outside the regions the guide shows.
    // NOLINTBEGIN(misc-no-recursion)
    // tag::grammar[]
    /** What `token` means at the start of an expression: a digit, or a minus sign. */
    template <class Parser>
    pratt::result<int> nud(Parser& p, char token) const {
        if (token >= '0' && token <= '9') {
            return token - '0';
        }
        if (token == '-') {
            const pratt::result<int> operand = p.expression(30);
            if (operand.has_error()) {
                return operand;
            }
            return -*operand;
        }
        if (token == '\0') {
            return pratt::error::unexpected_end;
        }
        return pratt::error::unexpected_token;
    }

    /** What `token` means after the expression `left`: a binary operator. */
    template <class Parser>
    pratt::result<int> led(Parser& p, char token, int left) const {
        const pratt::result<int> right = p.expression(lbp(token));
        if (right.has_error()) {
            return right;
        }
        switch (token) {
            case '+': return left + *right;
            case '-': return left - *right;
            default: return left * *right;
        }
    }

    // NOLINTEND(misc-no-recursion)
};

// end::grammar[]
}  // namespace

// tag::main[]
int main() {
    const arithmetic grammar;
    for (const std::string_view text : {"1+2*3", "2*3+4", "8-2-1", "-2*3", "1+", "1+*2", "12"}) {
        const pratt::result<int> value = pratt::parse(grammar, characters(text));
        if (value.has_error()) {
            std::cout << text << " -> error: " << value.error().message() << '\n';
        } else {
            std::cout << text << " -> " << *value << '\n';
        }
    }
}

// end::main[]
// end::program[]
