// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The engine: a Pratt parser that knows no operator. Every token is given its
 meaning by a grammar, through the token's nud, led and lbp.
*/
#ifndef WEBCPP_PRATT_PARSER_HPP
#define WEBCPP_PRATT_PARSER_HPP

#include <webcpp/pratt/errors.hpp>

#include <concepts>
#include <utility>

namespace webcpp::pratt {

/**
 The deepest nesting of @ref parser::expression calls a parse allows unless
 told otherwise.

 @note Provisional: the value is to be measured on wasm's default stack, in
 a debug build, with the calculator's frames.
*/
inline constexpr int default_max_depth = 64;

/**
 A source of tokens: the next one is read by `peek` and consumed by
 `advance`, and `at_end` says whether the input is spent.

 @note `peek` must stay valid at the end of the input, returning a token
 whose lbp is 0 and whose nud fails, such as an end token.
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
 never looks into.
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

    depth_guard(const depth_guard&) = delete;
    depth_guard& operator=(const depth_guard&) = delete;
    depth_guard(depth_guard&&) = delete;
    depth_guard& operator=(depth_guard&&) = delete;

private:
    int& depth_;
};

}  // namespace detail

/**
 A Pratt parser of the tokens of a `Stream`, by a `Grammar`.

 It holds a pointer to the grammar, the stream and the depth of its active
 @ref expression calls, and allocates nothing.

 @tparam Grammar A type that models @ref grammar_for of `Stream`.
 @tparam Stream A type that models @ref token_stream.
*/
template <class Grammar, class Stream>
class parser {
public:
    /** The tokens the stream yields. */
    using token_type = Stream::token_type;

    /** What the grammar builds. */
    using node_type = Grammar::node_type;

    /**
     Constructs a parser of `stream` by `grammar`, which must outlive it.

     @param grammar The grammar.
     @param stream The tokens.
     @param max_depth The deepest nesting of @ref expression calls allowed.
    */
    parser(const Grammar& grammar, Stream stream, int max_depth = default_max_depth)
        : grammar_(&grammar), stream_(std::move(stream)), max_depth_(max_depth) {}

    /** Refused: the parser keeps a pointer to its grammar, which a temporary would leave dangling.
     */
    parser(const Grammar&& grammar, Stream stream, int max_depth = default_max_depth) = delete;

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
    result<node_type> expression(int rbp) {
        if (depth_ >= max_depth_) {
            return error::nesting_too_deep;
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
            return error::trailing_input;
        }
        return node;
    }

    /** The next token, not consumed: for a nud or a led that reads past its own token. */
    [[nodiscard]] decltype(auto) peek() const { return stream_.peek(); }

    /** Consumes the next token and returns it: for a nud or a led that reads past its own token. */
    token_type advance() { return stream_.advance(); }

private:
    const Grammar* grammar_;
    Stream stream_;
    int max_depth_;
    int depth_ = 0;
};

/**
 Parses the whole of `stream` as one expression of `grammar`.

 @param grammar The grammar.
 @param stream The tokens.
 @param max_depth The deepest nesting of @ref parser::expression calls allowed.
 @return As @ref parser::parse returns.
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

/**
 The nud of an atom: the token is the whole expression.

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

 @param p The parser.
 @param bp The power the operand binds with.
 @param make Builds the node of the operator from its operand's, as a `result`.
 @return The node, or the operand's error.
*/
template <class Parser, class Make>
result<typename Parser::node_type> prefix(Parser& p, int bp, Make make) {
    result<typename Parser::node_type> operand = p.expression(bp);
    if (operand.has_error()) {
        return operand;
    }
    return make(std::move(*operand));
}

/**
 The led of a left-associative infix operator of power `bp`.

 @param p The parser.
 @param bp The operator's power.
 @param left The left operand's node.
 @param make Builds the node of the operator from its operands', as a `result`.
 @return The node, or the right operand's error.
*/
template <class Parser, class Node, class Make>
result<typename Parser::node_type> infix_left(Parser& p, int bp, Node left, Make make) {
    result<typename Parser::node_type> right = p.expression(bp);
    if (right.has_error()) {
        return right;
    }
    return make(std::move(left), std::move(*right));
}

/**
 The led of a right-associative infix operator of power `bp`.

 The right operand is parsed one step weaker, `bp - 1`, so that an equal
 operator on the right takes it.

 @param p The parser.
 @param bp The operator's power.
 @param left The left operand's node.
 @param make Builds the node of the operator from its operands', as a `result`.
 @return The node, or the right operand's error.
*/
template <class Parser, class Node, class Make>
result<typename Parser::node_type> infix_right(Parser& p, int bp, Node left, Make make) {
    result<typename Parser::node_type> right = p.expression(bp - 1);
    if (right.has_error()) {
        return right;
    }
    return make(std::move(left), std::move(*right));
}

/**
 The led of a postfix operator: its operand is the whole left expression.

 @param left The operand's node.
 @param make Builds the node of the operator from its operand's, as a `result`.
 @return What `make` returns.
*/
template <class Node, class Make>
auto postfix(Node left, Make make) -> decltype(make(std::move(left))) {
    return make(std::move(left));
}

/**
 The nud of an opening token: a whole inner expression, then the closing
 token that `is_close` accepts.

 @param p The parser.
 @param is_close Whether a token is the closing one.
 @return The inner expression's node, its error, or
 @ref error::expected_closing_token.
*/
template <class Parser, class IsClose>
result<typename Parser::node_type> grouping(Parser& p, IsClose is_close) {
    result<typename Parser::node_type> inner = p.expression(0);
    if (inner.has_error()) {
        return inner;
    }
    if (!is_close(p.peek())) {
        return error::expected_closing_token;
    }
    p.advance();
    return inner;
}

}  // namespace webcpp::pratt

#endif  // WEBCPP_PRATT_PARSER_HPP
