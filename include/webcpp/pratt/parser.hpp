// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The engine: a Pratt parser that knows no operator. Every token is given its
 meaning by a grammar, through the token's nud, led and lbp.

 @see "Writing a grammar", in the guide.
*/
#ifndef WEBCPP_PRATT_PARSER_HPP
#define WEBCPP_PRATT_PARSER_HPP

#include <webcpp/pratt/config.hpp>

#include <webcpp/pratt/errors.hpp>

#include <concepts>
#include <utility>

namespace webcpp::pratt {

// The measurement, which the guide's "Limits" repeats: the deepest N that ran without a trap,
// over calc::environment<>, with wasi-sdk 34 (clang 23.1.0) and wasmtime 47.0.3, and the stack
// divided by N.
//
// | Shape, N operators or `(` | wasip2, wasip3 `-O0` | wasip2 `-O2` | native `-O0`, 8 MiB |
// | ------------------------- | -------------------- | ------------ | ------------------- |
// | `((...(1)...))`           | 232 (282 B)          | 450 (146 B)  | 16854 (497 B)       |
// | `- - ... - 1`             | 263 (249 B)          | 450 (146 B)  | 18016 (465 B)       |
// | `2 ^ 2 ^ ... ^ 2`         | 240 (273 B)          | 405 (162 B)  | 12741 (657 B)       |
//
// Over every shape of the calculator's grammar and of openui-lang's, the deepest parse that runs
// in a debug build on wasm is at least 203 calls, openui-lang's call shape on wasip2; the
// calculator's least is 214, a grouping with a `pow` inside. Half of 203 is 101, and the largest
// power of two at most that is 64. In a debug build on wasm, a level of grouping costs 272 bytes,
// a level of `^` 256 and a level of sign 240, plus at most about 7.5 KiB that a parse needs at
// any depth, its heaviest operand, a `pow`, included.

/**
 The deepest nesting of @ref parser::expression calls a parse allows unless
 told otherwise.

 It is measured, not chosen: the largest power of two at most half the
 deepest parse that wasm's default stack of 64 KiB holds in a debug build,
 in the shape that costs the most of the calculator's grammar and of
 openui-lang's. A program that raises the limit raises its stack with it,
 with `-Wl,-z,stack-size=` on wasm and the thread's stack natively.

 @see "Limits", in the guide.
*/
inline constexpr int default_max_depth = 64;

/**
 A source of tokens: the next one is read by `peek` and consumed by
 `advance`, and `at_end` says whether the input is spent.

 The parser reads `peek` to decide whether the next token extends an
 expression, calls `advance` once for each token it hands a nud or a led,
 and asks `at_end` once the whole expression is parsed. A stream may lex
 lazily, one token ahead, as the calculator's lexer, `calc::stream`, does.

 @tparam S The stream's type, which names its tokens `token_type`.
 @note `peek` must stay valid at the end of the input, returning a token
 whose lbp is 0 and whose nud fails, such as an end token.
 @see "Writing a grammar", in the guide.
*/
template <class S>
concept token_stream = requires(S s, const S cs) {
    typename S::token_type;
    { cs.peek() } -> std::convertible_to<typename S::token_type>;
    { s.advance() } -> std::same_as<typename S::token_type>;
    { cs.at_end() } -> std::convertible_to<bool>;
};

template <class Grammar, class Stream>
class parser;

/**
 A grammar of the tokens of `S`: for every token, its nud (its meaning at
 the start of an expression), its led (its meaning after a left operand) and
 its lbp (how tightly it binds that left operand).

 The node a parse builds is the grammar's `node_type`, which the library
 never looks into. A nud and a led receive the parser, so that they parse
 their operands through @ref parser::expression or the helpers, and return
 the node or an error. A led is called only for a token whose lbp is greater
 than the binding power the expression is parsed with, so a token of lbp 0,
 such as an end token or a closing parenthesis, ends every expression parsed
 with a binding power of 0 or more.

 @tparam G The grammar's type, which names its node `node_type`.
 @tparam S The type of the token stream, which models @ref token_stream.
 @see "Writing a grammar", in the guide.
*/
template <class G, class S>
concept grammar_for = token_stream<S> &&
                      requires(const G g, parser<G, S>& p, const S::token_type& t, G::node_type n) {
                          typename G::node_type;
                          { g.lbp(t) } -> std::convertible_to<int>;
                          { g.nud(p, t) } -> std::same_as<result<typename G::node_type>>;
                          {
                              g.led(p, t, std::move(n))
                          } -> std::same_as<result<typename G::node_type>>;
                      };

namespace detail {

/** Counts one active @ref parser::expression call for as long as it lives. */
class depth_guard {
public:
    /** Counts the call in `depth`. */
    explicit depth_guard(int& depth) noexcept : depth_(depth) { ++depth_; }

    /** Uncounts the call, on whichever path it returns. */
    ~depth_guard() { --depth_; }

    /** Refused: a copy would uncount the call a second time. */
    depth_guard(const depth_guard&) = delete;

    /** Refused: the guard counts one call for as long as it lives. */
    depth_guard& operator=(const depth_guard&) = delete;

    /** Refused: a moved-from guard would still uncount the call. */
    depth_guard(depth_guard&&) = delete;

    /** Refused: the guard counts one call for as long as it lives. */
    depth_guard& operator=(depth_guard&&) = delete;

private:
    int& depth_;
};

}  // namespace detail

/**
 A Pratt parser of the tokens of a `Stream`, by a `Grammar`.

 It holds a pointer to the grammar, the stream and the depth of its active
 @ref expression calls, and allocates nothing.

 Its depth limit, `max_depth`, bounds the recursion through @ref expression
 alone. That is the whole recursion of a grammar whose nud and led parse
 every operand through @ref expression or the helpers, as the calculator's
 do. A nud or a led that recurses through functions of its own grammar,
 without calling @ref expression, is not bounded by it, and must bound that
 recursion itself.

 @tparam Grammar A type that models @ref grammar_for of `Stream`.
 @tparam Stream A type that models @ref token_stream.
 @see "Writing a grammar", in the guide.
 @see "Limits", in the guide.
*/
template <class Grammar, class Stream>
class parser {
public:
    /** The tokens the stream yields. */
    using token_type = Stream::token_type;

    /** What the grammar builds. */
    using node_type = Grammar::node_type;

    /**
     Constructs a parser of `stream` by `grammar`.

     @param grammar The grammar.
     @param stream The tokens.
     @param max_depth The deepest nesting of @ref expression calls allowed.
     A negative value behaves as 0, which refuses every parse with
     @ref error::nesting_too_deep.
     @pre `grammar` outlives the parser.
    */
    parser(const Grammar& grammar, Stream stream, int max_depth = default_max_depth)
        : grammar_(&grammar), stream_(std::move(stream)), max_depth_(max_depth) {}

    /**
     Refused: the parser keeps a pointer to its grammar, which a temporary
     would leave dangling.
    */
    parser(const Grammar&& grammar, Stream stream, int max_depth = default_max_depth) = delete;

    // expression() recurses through the grammar's nud and led and the helpers below: that recursion
    // is Pratt parsing, and max_depth bounds it, hence the exemption from misc-no-recursion.

    /**
     Parses the longest expression whose operators bind tighter than `rbp`.

     The first token's nud parses the start, and while the next token's lbp
     is greater than `rbp`, that token's led extends the expression.

     @param rbp The right binding power of the operator that asks for the
     expression, 0 for a whole one.
     @return The expression's node, the first error of a nud or a led, or
     @ref error::nesting_too_deep, before any token is read, from the call
     that would make more than `max_depth` calls active.
    */
    result<node_type> expression(int rbp) {  // NOLINT(misc-no-recursion)
        if (depth_ >= max_depth_) {
            // In place, as every error of the engine: a node type may convert from an error too.
            return {boost::system::in_place_error, error::nesting_too_deep};
        }
        const detail::depth_guard guard(depth_);
        token_type token = stream_.advance();
        result<node_type> left = grammar_->nud(*this, token);
        if (left.has_error()) {
            return left;
        }
        while (grammar_->lbp(stream_.peek()) > rbp) {
            token = stream_.advance();
            left = grammar_->led(*this, token, std::move(*left));
            if (left.has_error()) {
                return left;
            }
        }
        return left;
    }

    /**
     Parses the whole input as one expression.

     @return The expression's node, its error, or @ref error::trailing_input
     when tokens remain after it.
    */
    result<node_type> parse() {
        result<node_type> node = expression(0);
        if (node.has_error()) {
            return node;
        }
        if (!stream_.at_end()) {
            return {boost::system::in_place_error, error::trailing_input};
        }
        return node;
    }

    /**
     The next token, not consumed: for a nud or a led that reads past its own token.

     @return What the stream's `peek` returns.
    */
    [[nodiscard]] decltype(auto) peek() const { return stream_.peek(); }

    /**
     Consumes the next token and returns it: for a nud or a led that reads past its own token.

     @return The token consumed.
    */
    token_type advance() { return stream_.advance(); }

private:
    const Grammar* grammar_;
    Stream stream_;
    int max_depth_;
    int depth_ = 0;
};

/**
 Parses the whole of `stream` as one expression of `grammar`.

 @tparam Grammar A type that models @ref grammar_for of `Stream`.
 @tparam Stream A type that models @ref token_stream.
 @param grammar The grammar.
 @param stream The tokens.
 @param max_depth The deepest nesting of @ref parser::expression calls
 allowed, as @ref parser takes it: a negative value behaves as 0.
 @return As @ref parser::parse returns.
 @see "Writing a grammar", in the guide.
*/
template <class Grammar, token_stream Stream>
    requires grammar_for<Grammar, Stream>
result<typename Grammar::node_type> parse(const Grammar& grammar, Stream stream,
                                          int max_depth = default_max_depth) {
    parser<Grammar, Stream> instance(grammar, std::move(stream), max_depth);
    return instance.parse();
}

// The category helpers. Each writes one category of operator through the parser's expression()
// recursion alone, so a grammar built from them still routes every token through nud and led.
// Those that recurse are exempt from misc-no-recursion as expression() is, on the line above
// their name.

/**
 The nud of an atom: the token is the whole expression.

 @tparam Make A callable of `const Token&`.
 @tparam Token The token's type.
 @param make Builds the node of `token`, as a `result`.
 @param token The atom.
 @return What `make` returns.
*/
template <class Make, class Token>
auto literal(Make make, const Token& token) -> decltype(make(token)) {
    return make(token);
}

/**
 The nud of a prefix operator, whose operand binds with power `bp`.

 @tparam Parser The parser's type, a @ref parser.
 @tparam Make A callable of the operand's node.
 @param p The parser.
 @param bp The power the operand binds with.
 @param make Builds the node of the operator from its operand's, as a `result`.
 @return The node, or the operand's error.
*/
template <class Parser, class Make>
// NOLINTNEXTLINE(misc-no-recursion)
result<typename Parser::node_type> prefix(Parser& p, int bp, Make make) {
    result<typename Parser::node_type> operand = p.expression(bp);
    if (operand.has_error()) {
        return operand;
    }
    return make(std::move(*operand));
}

/**
 The led of a left-associative infix operator of power `bp`.

 @tparam Parser The parser's type, a @ref parser.
 @tparam Node The node's type, the parser's `node_type`, as the led passes it.
 @tparam Make A callable of the two operands' nodes.
 @param p The parser.
 @param bp The operator's power.
 @param left The left operand's node, which `make` receives as it is passed:
 a led that owns it moves it in.
 @param make Builds the node of the operator from its operands', as a `result`.
 @return The node, or the right operand's error.
*/
template <class Parser, class Node, class Make>
// NOLINTNEXTLINE(misc-no-recursion)
result<typename Parser::node_type> infix_left(Parser& p, int bp, Node&& left, Make make) {
    result<typename Parser::node_type> right = p.expression(bp);
    if (right.has_error()) {
        return right;
    }
    return make(std::forward<Node>(left), std::move(*right));
}

/**
 The led of a right-associative infix operator of power `bp`.

 The right operand is parsed one step weaker, `bp - 1`, so that an equal
 operator on the right takes it.

 @tparam Parser The parser's type, a @ref parser.
 @tparam Node The node's type, the parser's `node_type`, as the led passes it.
 @tparam Make A callable of the two operands' nodes.
 @param p The parser.
 @param bp The operator's power.
 @param left The left operand's node, which `make` receives as it is passed:
 a led that owns it moves it in.
 @param make Builds the node of the operator from its operands', as a `result`.
 @return The node, or the right operand's error.
*/
template <class Parser, class Node, class Make>
// NOLINTNEXTLINE(misc-no-recursion)
result<typename Parser::node_type> infix_right(Parser& p, int bp, Node&& left, Make make) {
    result<typename Parser::node_type> right = p.expression(bp - 1);
    if (right.has_error()) {
        return right;
    }
    return make(std::forward<Node>(left), std::move(*right));
}

/**
 The led of a postfix operator: its operand is the whole left expression.

 @tparam Node The node's type, the parser's `node_type`, as the led passes it.
 @tparam Make A callable of the operand's node.
 @param left The operand's node, which `make` receives as it is passed.
 @param make Builds the node of the operator from its operand's, as a `result`.
 @return What `make` returns.
*/
template <class Node, class Make>
auto postfix(Node&& left, Make make) -> decltype(make(std::forward<Node>(left))) {
    return make(std::forward<Node>(left));
}

/**
 The nud of an opening token: a whole inner expression, then the closing
 token that `is_close` accepts.

 @tparam Parser The parser's type, a @ref parser.
 @tparam IsClose A predicate of the parser's `token_type`.
 @param p The parser.
 @param is_close Whether a token is the closing one.
 @return The inner expression's node, its error, or
 @ref error::expected_closing_token.
*/
template <class Parser, class IsClose>
// NOLINTNEXTLINE(misc-no-recursion)
result<typename Parser::node_type> grouping(Parser& p, IsClose is_close) {
    result<typename Parser::node_type> inner = p.expression(0);
    if (inner.has_error()) {
        return inner;
    }
    if (!is_close(p.peek())) {
        return {boost::system::in_place_error, error::expected_closing_token};
    }
    p.advance();
    return inner;
}

}  // namespace webcpp::pratt

#endif  // WEBCPP_PRATT_PARSER_HPP
