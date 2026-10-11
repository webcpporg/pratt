# pratt

pratt is a header-only engine for Pratt parsing, top-down operator
precedence, in C++20, one of the libraries of
[webcpp](https://github.com/webcpporg/webcpp), with a calculator built on it.

The engine knows no token, no operator and no tree. A program gives it a
token stream and a grammar that says, for each token, what it means at the
start of an expression (its nud), what it means after one (its led), and how
tightly it binds the expression on its left (its lbp); the engine returns
the grammar's node, or an error, as a value. Helpers write the usual kinds
of operator: literals, prefix, left- and right-associative infix, postfix,
and groupings. The engine allocates nothing, throws nothing, and bounds its
recursion with a depth limit, so a deeply nested input is an error rather
than a stack overflow.

The calculator, `<webcpp/pratt/calculator.hpp>`, evaluates numbers,
variables and C's arithmetic and comparison operators, exactly in
`boost::decimal::decimal64_t` by default, or in any number type that models
its concept, with its variables in an environment whose allocator is the
program's.

`<webcpp/pratt.hpp>` includes the engine, whose names are in the namespace
`webcpp::pratt`; the calculator's are in `webcpp::pratt::calc`. pratt needs
Boost's headers and webcpp's configuration, config, and nothing to build or
link. It runs natively, on emscripten, on wasm32-wasip2 and on
wasm32-wasip3.

## A minimal example

This is [example/pratt_first_grammar.cpp](example/pratt_first_grammar.cpp),
a grammar of one-digit integers with `+`, `-`, `*` and a unary minus, which
computes each value while it parses:

<!-- include::example/pratt_first_grammar.cpp[tag=program] -->
```cpp
#include <webcpp/pratt.hpp>

#include <cstddef>
#include <iostream>
#include <string_view>

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

    // nud() and led() recurse through the parser's expression(), which max_depth bounds, hence the
    // exemption from misc-no-recursion, outside the regions the guide shows.
    // NOLINTBEGIN(misc-no-recursion)
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

}  // namespace

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
```

It prints:

<!-- include::example/pratt_first_grammar.expected[] -->
```
1+2*3 -> 7
2*3+4 -> 10
8-2-1 -> 5
-2*3 -> -6
1+ -> error: unexpected end of input
1+*2 -> error: unexpected token
12 -> error: trailing input after expression
```

## Building and testing

pratt is developed inside the webcpp superproject, as a Boost library is
developed inside Boost:

    git clone --recursive https://github.com/webcpporg/webcpp
    cd webcpp
    b2 libs/pratt/test libs/pratt/example

builds the tests and the examples natively, runs every example and compares
its output with the `.expected` file beside it. `toolset=emscripten` does
the same on emscripten, run by node, with the toolset that `user-config.jam`
registers against emsdk.
`toolset=clang-wasip2 testing.launcher=wasmtime` and
`toolset=clang-wasip3 testing.launcher=wasmtime` do the same for WASI, with
the toolsets that `user-config.jam` registers against wasi-sdk. A b2 project
uses pratt through `/webcpp/pratt//pratt`.

## Documentation

pratt's page, with its API reference, is published at
<https://webcpporg.github.io/libs/pratt/>; `b2 libs/pratt/doc`
builds it into `doc/html/index.html`.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
