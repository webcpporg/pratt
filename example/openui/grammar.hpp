// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A micro-subset of openui-lang on the engine: identifiers, integers,
 double-quoted strings, the right-associative assignment `=`, the named
 property `:` and the call `Component(arg, ...)`. A parse is written back as
 an S-expression: `root = Stack(gap: 4)` is `(= root (call Stack (: gap 4)))`.

 Tip: `(` holds both of Pratt's denotations, its nud a grouping and its led a
 call whose argument loop lives inside the led. Every token goes through the
 engine; nothing descends recursively around it.
*/
#ifndef WEBCPP_EXAMPLE_PRATT_OPENUI_GRAMMAR_HPP
#define WEBCPP_EXAMPLE_PRATT_OPENUI_GRAMMAR_HPP

#include <webcpp/pratt/parser.hpp>

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace webcpp::example::openui {

/** What a token is. */
enum class kind {
    identifier,
    integer,
    string,
    lparen,
    rparen,
    comma,
    colon,
    assign,
    /** A character no token starts with, one character long. */
    invalid,
    /** A `"` whose string the line does not close, up to the end of the line. */
    unterminated,
    /** The end of the input, which an empty text at the end of the source spells. */
    end,
};

/** A token of the input. */
struct token {
    /** What the token is. */
    kind type = kind::end;

    /** The token's characters, a view into the source; a string's include its quotes. */
    std::string_view text{};
};

namespace detail {

/** The kind of the one-character token `c`, or @ref kind::invalid when `c` starts none. */
constexpr kind punctuation_kind(char c) noexcept {
    switch (c) {
        case '(': return kind::lparen;
        case ')': return kind::rparen;
        case ',': return kind::comma;
        case ':': return kind::colon;
        case '=': return kind::assign;
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
    return c == ' ' || c == '\t' || c == '\r';
}

/**
 Builds the node `(<op> left right)` of the binary operator `op` from its
 operands' nodes, as the engine's infix helpers call it.
*/
inline auto binary(std::string_view op) {
    return [op](std::string left, std::string right) -> pratt::result<std::string> {
        return "(" + std::string(op) + " " + std::move(left) + " " + std::move(right) + ")";
    };
}

}  // namespace detail

/**
 The lexer, a lazy @ref webcpp::pratt::token_stream. It holds the source of
 one line, its position and one token of lookahead, and lexes the next token
 only when @ref advance consumes the current one.

 An identifier is a letter or `_`, then letters, digits and `_`; an integer
 is digits; a string is `"`, any characters but `"`, and `"`, with no
 escapes. Space, tab and carriage return separate tokens. Any other
 character is a token of kind @ref kind::invalid, and a `"` the line does
 not close is a token of kind @ref kind::unterminated, which runs to the end
 of the line; the grammar rejects each where it stands.
*/
class stream {
public:
    /** The tokens the stream yields. */
    using token_type = token;

    /**
     Constructs the stream of `source`, which must outlive it and its tokens,
     and lexes the first token.
    */
    explicit stream(std::string_view source) : source_(source), next_(lex()) {}

    /** The next token, not consumed; the end token once the input is spent. */
    [[nodiscard]] const token& peek() const { return next_; }

    /** Consumes the next token and returns it; at the end, returns the end token again. */
    token advance() {
        const token current = next_;
        if (current.type == kind::end) {
            return current;
        }
        next_ = lex();
        return current;
    }

    /** Whether the input is spent: the next token is the end token. */
    [[nodiscard]] bool at_end() const { return next_.type == kind::end; }

private:
    /** Lexes the token at the position, after the whitespace before it, and moves past it. */
    token lex() {
        skip_whitespace();
        if (pos_ >= source_.size()) {
            return token{.type = kind::end, .text = source_.substr(pos_)};
        }
        const char c = source_[pos_];
        if (detail::is_name_start(c)) {
            return lex_while(kind::identifier, [](char n) {
                return detail::is_name_start(n) || detail::is_digit(n);
            });
        }
        if (detail::is_digit(c)) {
            return lex_while(kind::integer, detail::is_digit);
        }
        if (c == '"') {
            return lex_string();
        }
        const std::size_t start = pos_;
        ++pos_;
        return token{.type = detail::punctuation_kind(c), .text = source_.substr(start, 1)};
    }

    void skip_whitespace() {
        while (pos_ < source_.size() && detail::is_whitespace(source_[pos_])) {
            ++pos_;
        }
    }

    /**
     Lexes a token of kind `type`: the character at the position, and those after it `more` accepts.
    */
    template <class More>
    token lex_while(kind type, More more) {
        const std::size_t start = pos_;
        ++pos_;
        while (pos_ < source_.size() && more(source_[pos_])) {
            ++pos_;
        }
        return token{.type = type, .text = source_.substr(start, pos_ - start)};
    }

    /** Lexes a string at the position, which is a `"`, or the unterminated rest of the line. */
    token lex_string() {
        const std::size_t start = pos_;
        const std::size_t close = source_.find('"', start + 1);
        if (close == std::string_view::npos) {
            pos_ = source_.size();
            return token{.type = kind::unterminated, .text = source_.substr(start)};
        }
        pos_ = close + 1;
        return token{.type = kind::string, .text = source_.substr(start, pos_ - start)};
    }

    std::string_view source_;
    std::size_t pos_ = 0;
    token next_;
};

/**
 The grammar of the micro-subset, whose node is the S-expression of the
 parse.

 | Token               | nud            | led                                   | lbp         |
 | ------------------- | -------------- | ------------------------------------- | ----------- |
 | identifier, integer | itself         |                                       | 0           |
 | string              | itself, quoted |                                       | 0           |
 | `(`                 | a grouping     | a call, `(call f a ...)`              | `call_bp`   |
 | `:`                 |                | `(: name value)`, left-associative    | `pair_bp`   |
 | `=`                 |                | `(= target value)`, right-associative | `assign_bp` |
*/
struct grammar {
    /** A parse, written as an S-expression. */
    using node_type = std::string;

    /** The power of `=`, the loosest operator. */
    static constexpr int assign_bp = 10;

    /** The power of `:`, below a call and above `=`, so `a = b: c` is `(= a (: b c))`. */
    static constexpr int pair_bp = 20;

    /** The power of a call, the tightest operator, so `Stack(x)(y)` calls the call. */
    static constexpr int call_bp = 90;

    /**
     The power of an invalid character and of an unterminated string, higher
     than every other, so that one after a complete operand is read as an
     operator, whose led gives its error, rather than end the expression with
     @ref webcpp::pratt::error::trailing_input.
    */
    static constexpr int invalid_bp = std::numeric_limits<int>::max();

    /**
     How tightly the token `t` binds the expression on its left.

     @param t The token.
     @return Its power, as the constants above give it; 0 for a token that
     takes no left operand.
    */
    [[nodiscard]] static int lbp(const token& t) {
        switch (t.type) {
            case kind::assign: return assign_bp;
            case kind::colon: return pair_bp;
            case kind::lparen: return call_bp;
            case kind::invalid:
            case kind::unterminated: return invalid_bp;
            default: return 0;
        }
    }

    // nud(), led() and call() recurse through the engine's expression(): that is Pratt parsing,
    // bounded by the parser's max_depth, hence the exemption from misc-no-recursion.

    /**
     The meaning of the token `t` at the start of an expression.

     @param p The parser.
     @param t The token.
     @return The node of an atom or of a grouping, or the error of the token
     or of the grouping.
    */
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<node_type> nud(Parser& p, const token& t) const {
        switch (t.type) {
            case kind::identifier:
            case kind::integer:
            case kind::string:
                return pratt::literal(
                    [](const token& atom) -> pratt::result<node_type> {
                        return node_type(atom.text);
                    },
                    t);
            case kind::lparen:
                return pratt::grouping(p,
                                       [](const token& next) { return next.type == kind::rparen; });
            case kind::invalid: return pratt::error::invalid_character;
            case kind::unterminated:
            case kind::end: return pratt::error::unexpected_end;
            default: return pratt::error::unexpected_token;
        }
    }

    /**
     The meaning of the token `t` after the expression `left`.

     @param p The parser.
     @param t The token.
     @param left The expression on its left.
     @return The node of the assignment, of the pair or of the call, or the
     error of the token or of its operands.
    */
    // NOLINTBEGIN(misc-no-recursion)
    // tag::led[]
    template <class Parser>
    pratt::result<node_type> led(Parser& p, const token& t, node_type left) const {
        switch (t.type) {
            case kind::assign:
                return pratt::infix_right(p, assign_bp, std::move(left), detail::binary("="));
            case kind::colon:
                return pratt::infix_left(p, pair_bp, std::move(left), detail::binary(":"));
            case kind::lparen: return call(p, std::move(left));
            case kind::invalid: return pratt::error::invalid_character;
            case kind::unterminated: return pratt::error::unexpected_end;
            default: return pratt::error::unexpected_token;
        }
    }

    // end::led[]
    // NOLINTEND(misc-no-recursion)

private:
    /**
     The led of a call of `callee`, whose `(` is consumed: its arguments,
     each a whole expression, separated by `,` and closed by `)`.

     Tip: `,` has lbp 0, so it ends the expression of an argument; the loop
     reads it here, inside the led, and never descends around the engine.
    */
    // NOLINTBEGIN(misc-no-recursion)
    // tag::call[]
    template <class Parser>
    static pratt::result<node_type> call(Parser& p, node_type callee) {
        node_type node = "(call " + std::move(callee);
        if (p.peek().type == kind::rparen) {
            p.advance();
            return std::move(node) + ")";
        }
        while (true) {
            pratt::result<node_type> argument = p.expression(0);
            if (argument.has_error()) {
                return argument;
            }
            node += ' ';
            node += *argument;
            const kind next = p.peek().type;
            if (next == kind::rparen) {
                p.advance();
                return std::move(node) + ")";
            }
            if (next != kind::comma) {
                return pratt::error::expected_closing_token;
            }
            p.advance();
        }
    }

    // end::call[]
    // NOLINTEND(misc-no-recursion)
};

}  // namespace webcpp::example::openui

#endif  // WEBCPP_EXAMPLE_PRATT_OPENUI_GRAMMAR_HPP
