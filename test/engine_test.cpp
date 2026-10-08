// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests the engine with a grammar of the test's own, whose binding powers come from a table:
// precedence, associativity, prefix and postfix positions, the errors the engine and its helpers
// report, the order of the nud and led calls, and the bound on the depth of the recursion.

#include <webcpp/pratt/parser.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "require.hpp"
#include "sexpr.hpp"

namespace pratt = webcpp::pratt;
namespace sexpr = webcpp::test::sexpr;

using webcpp::test::require;

namespace {

/** How an operator of the table extends the expression on its left. */
enum class position : char { infix_left, infix_right, postfix };

/** An operator of the table: its token, its left binding power and its position. */
struct binding {
    char token;
    int power;
    position kind;
};

/** The test grammar's operators. */
constexpr std::array bindings{
    binding{.token = '+', .power = 10, .kind = position::infix_left},
    binding{.token = '-', .power = 10, .kind = position::infix_left},
    binding{.token = '*', .power = 20, .kind = position::infix_left},
    binding{.token = '^', .power = 30, .kind = position::infix_right},
    binding{.token = '!', .power = 40, .kind = position::postfix},
};

/** The power the operand of a prefix `-` binds with: between `*` and `^`, so `-2^2` is `-(2^2)`. */
constexpr int negation_power = 25;

/** The row of `token`, or none for a token that is no operator. */
const binding* find_binding(char token) {
    for (const binding& row : bindings) {
        if (row.token == token) {
            return &row;
        }
    }
    return nullptr;
}

/** Letters and digits as atoms, `( )`, prefix `-`, and the operators of the table. */
struct table {
    using node_type = std::string;

    [[nodiscard]] static int lbp(char token) {
        const binding* row = find_binding(token);
        if (row == nullptr) {
            return 0;
        }
        return row->power;
    }

    // Recurses through the engine's expression(): Pratt's recursion, bounded by max_depth.
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<std::string> nud(Parser& parser, char token) const {
        if ((token >= 'a' && token <= 'z') || (token >= '0' && token <= '9')) {
            return pratt::literal(
                [](char atom) -> pratt::result<std::string> { return std::string(1, atom); },
                token);
        }
        if (token == '(') {
            return pratt::grouping(parser, [](char next) { return next == ')'; });
        }
        if (token == '-') {
            return pratt::prefix(parser, negation_power,
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
        const binding* row = find_binding(token);
        if (row == nullptr) {
            return pratt::error::unexpected_token;
        }
        const auto join = [token](std::string&& l, std::string&& r) -> pratt::result<std::string> {
            return "(" + std::string(1, token) + " " + std::move(l) + " " + std::move(r) + ")";
        };
        switch (row->kind) {
            case position::infix_left:
                return pratt::infix_left(parser, row->power, std::move(left), join);
            case position::infix_right:
                return pratt::infix_right(parser, row->power, std::move(left), join);
            case position::postfix:
                return pratt::postfix(
                    std::move(left), [token](std::string&& operand) -> pratt::result<std::string> {
                        return "(" + std::string(1, token) + " " + std::move(operand) + ")";
                    });
        }
        return pratt::error::unexpected_token;
    }
};

/** The table grammar, writing down every nud and led call before it makes it. */
class recording {
public:
    using node_type = std::string;

    explicit recording(std::vector<std::string>& calls) : calls_(&calls) {}

    [[nodiscard]] static int lbp(char token) { return table::lbp(token); }

    // Recurses through the engine's expression(): Pratt's recursion, bounded by max_depth.
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<std::string> nud(Parser& parser, char token) const {
        calls_->push_back(std::string("nud ") + token);
        return inner_.nud(parser, token);
    }

    // Recurses through the engine's expression(): Pratt's recursion, bounded by max_depth.
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<std::string> led(Parser& parser, char token, std::string left) const {
        calls_->push_back(std::string("led ") + token);
        return inner_.led(parser, token, std::move(left));
    }

private:
    std::vector<std::string>* calls_;
    table inner_;
};

/**
 A node that converts from any type, as a Boost.Multiprecision number with et_off does, and so
 from pratt::error too.

 Tip: boost::system::result<T> refuses to convert from a value that converts both to its T and to
 an error_code, so only an error built in place, with in_place_error, reaches this node's result.
*/
struct convertible_from_anything {
    template <class T>
    convertible_from_anything(const T& /*value*/) {}  // NOLINT(*-explicit-*)
};

static_assert(std::is_convertible_v<pratt::error, convertible_from_anything>);

/** Atoms and `( )`, whose node converts from anything: what the engine's own errors reach. */
struct accepting {
    using node_type = convertible_from_anything;

    [[nodiscard]] static int lbp(char /*token*/) { return 0; }

    // Recurses through the engine's expression(): Pratt's recursion, bounded by max_depth.
    template <class Parser>
    // NOLINTNEXTLINE(misc-no-recursion)
    pratt::result<node_type> nud(Parser& parser, char token) const {
        if (token == '(') {
            return pratt::grouping(parser, [](char next) { return next == ')'; });
        }
        return node_type(token);
    }

    template <class Parser>
    pratt::result<node_type> led(Parser& /*parser*/, char /*token*/, node_type /*left*/) const {
        return {boost::system::in_place_error, pratt::error::unexpected_token};
    }
};

using table_parser = pratt::parser<table, sexpr::stream>;

static_assert(pratt::token_stream<sexpr::stream>);
static_assert(pratt::grammar_for<table, sexpr::stream>);
static_assert(pratt::grammar_for<recording, sexpr::stream>);
static_assert(pratt::grammar_for<accepting, sexpr::stream>);

// A parser keeps a pointer to its grammar, so it refuses a temporary one.
static_assert(!std::is_constructible_v<table_parser, table, sexpr::stream>);
static_assert(std::is_constructible_v<table_parser, const table&, sexpr::stream>);

// Each type below lacks one member of a complete one. A concept reads declarations alone, so no
// member is defined; nud and led take the one parser the concept passes, not a template
// parameter, because an unused template of this file is a warning (-Wunused-template).

/** A stream with every member, which the incomplete ones below are compared with. */
struct complete_stream {
    using token_type = char;
    [[nodiscard]] char peek() const;
    char advance();
    [[nodiscard]] bool at_end() const;
};

/** A stream without `peek`. */
struct stream_without_peek {
    using token_type = char;
    char advance();
    [[nodiscard]] bool at_end() const;
};

/** A stream without `advance`. */
struct stream_without_advance {
    using token_type = char;
    [[nodiscard]] char peek() const;
    [[nodiscard]] bool at_end() const;
};

/** A stream without `at_end`. */
struct stream_without_at_end {
    using token_type = char;
    [[nodiscard]] char peek() const;
    char advance();
};

static_assert(pratt::token_stream<complete_stream>);
static_assert(!pratt::token_stream<stream_without_peek>);
static_assert(!pratt::token_stream<stream_without_advance>);
static_assert(!pratt::token_stream<stream_without_at_end>);

/** A grammar with every member, which the incomplete ones below are compared with. */
struct complete_grammar {
    using node_type = std::string;
    [[nodiscard]] int lbp(char token) const;
    pratt::result<std::string> nud(pratt::parser<complete_grammar, sexpr::stream>& parser,
                                   char token) const;
    pratt::result<std::string> led(pratt::parser<complete_grammar, sexpr::stream>& parser,
                                   char token, std::string left) const;
};

/** A grammar without `nud`. */
struct grammar_without_nud {
    using node_type = std::string;
    [[nodiscard]] int lbp(char token) const;
    pratt::result<std::string> led(pratt::parser<grammar_without_nud, sexpr::stream>& parser,
                                   char token, std::string left) const;
};

/** A grammar without `led`. */
struct grammar_without_led {
    using node_type = std::string;
    [[nodiscard]] int lbp(char token) const;
    pratt::result<std::string> nud(pratt::parser<grammar_without_led, sexpr::stream>& parser,
                                   char token) const;
};

/** A grammar without `lbp`. */
struct grammar_without_lbp {
    using node_type = std::string;
    pratt::result<std::string> nud(pratt::parser<grammar_without_lbp, sexpr::stream>& parser,
                                   char token) const;
    pratt::result<std::string> led(pratt::parser<grammar_without_lbp, sexpr::stream>& parser,
                                   char token, std::string left) const;
};

/** A grammar without `node_type`. */
struct grammar_without_node_type {
    [[nodiscard]] int lbp(char token) const;
    pratt::result<std::string> nud(pratt::parser<grammar_without_node_type, sexpr::stream>& parser,
                                   char token) const;
    pratt::result<std::string> led(pratt::parser<grammar_without_node_type, sexpr::stream>& parser,
                                   char token, std::string left) const;
};

static_assert(pratt::grammar_for<complete_grammar, sexpr::stream>);
static_assert(!pratt::grammar_for<grammar_without_nud, sexpr::stream>);
static_assert(!pratt::grammar_for<grammar_without_led, sexpr::stream>);
static_assert(!pratt::grammar_for<grammar_without_lbp, sexpr::stream>);
static_assert(!pratt::grammar_for<grammar_without_node_type, sexpr::stream>);

void parses_to(std::string_view source, std::string_view expected) {
    const pratt::result<std::string> parsed = pratt::parse(table{}, sexpr::stream(source));
    require(BOOST_TEST(parsed.has_value()));
    BOOST_TEST_EQ(*parsed, expected);
}

void fails_with(std::string_view source, pratt::error expected) {
    const pratt::result<std::string> parsed = pratt::parse(table{}, sexpr::stream(source));
    require(BOOST_TEST(parsed.has_error()));
    BOOST_TEST(parsed.error() == expected);
}

/** `count` opening parentheses, `a`, and `count` closing ones. */
std::string nested(int count) {
    const auto size = static_cast<std::string::size_type>(count);
    return std::string(size, '(') + "a" + std::string(size, ')');
}

void a_tighter_operator_takes_the_operand_between() {
    parses_to("a+b*c", "(+ a (* b c))");
    parses_to("a*b+c", "(+ (* a b) c)");
}

void an_infix_left_operator_associates_to_the_left() {
    parses_to("a-b-c", "(- (- a b) c)");
}

void an_infix_right_operator_associates_to_the_right() {
    parses_to("a^b^c", "(^ a (^ b c))");
}

void a_prefix_operator_binds_with_its_own_power() {
    parses_to("-2^2", "(neg (^ 2 2))");
    parses_to("-a*b", "(* (neg a) b)");
}

void a_postfix_operator_takes_everything_that_binds_tighter() {
    parses_to("a!", "(! a)");
    parses_to("a+b!", "(+ a (! b))");
    parses_to("-a!", "(neg (! a))");
}

void a_grouping_needs_its_closing_token() {
    parses_to("(a+b)*c", "(* (+ a b) c)");
    fails_with("(a+b", pratt::error::expected_closing_token);
}

void the_engine_and_the_grammar_report_errors() {
    fails_with("", pratt::error::unexpected_end);
    fails_with("a+", pratt::error::unexpected_end);
    fails_with(")", pratt::error::unexpected_token);
    fails_with("*a", pratt::error::unexpected_token);
    fails_with("a)", pratt::error::trailing_input);
    fails_with("ab", pratt::error::trailing_input);
}

void expression_dispatches_to_nud_and_led_only() {
    std::vector<std::string> calls;
    const recording grammar(calls);
    const pratt::result<std::string> parsed = pratt::parse(grammar, sexpr::stream("a+b"));
    require(BOOST_TEST(parsed.has_value()));
    BOOST_TEST_EQ(*parsed, "(+ a b)");
    const std::vector<std::string> expected{"nud a", "led +", "nud b"};
    BOOST_TEST_ALL_EQ(calls.begin(), calls.end(), expected.begin(), expected.end());
}

void the_depth_is_bounded_by_max_depth() {
    const pratt::result<std::string> deep = pratt::parse(table{}, sexpr::stream(nested(4)), 4);
    require(BOOST_TEST(deep.has_error()));
    BOOST_TEST(deep.error() == pratt::error::nesting_too_deep);

    const pratt::result<std::string> within = pratt::parse(table{}, sexpr::stream(nested(3)), 4);
    require(BOOST_TEST(within.has_value()));
    BOOST_TEST_EQ(*within, "a");

    const pratt::result<std::string> none = pratt::parse(table{}, sexpr::stream("a"), 0);
    require(BOOST_TEST(none.has_error()));
    BOOST_TEST(none.error() == pratt::error::nesting_too_deep);

    // A negative limit behaves as 0.
    const pratt::result<std::string> negative = pratt::parse(table{}, sexpr::stream("a"), -1);
    require(BOOST_TEST(negative.has_error()));
    BOOST_TEST(negative.error() == pratt::error::nesting_too_deep);
}

/** Parses `source` with `max_depth` and expects `expected`. */
void parses_within(std::string_view source, int max_depth, std::string_view expected) {
    const pratt::result<std::string> parsed =
        pratt::parse(table{}, sexpr::stream(source), max_depth);
    require(BOOST_TEST(parsed.has_value()));
    BOOST_TEST_EQ(*parsed, expected);
}

/** Parses `source` with `max_depth` and expects it refused as too deep. */
void too_deep_within(std::string_view source, int max_depth) {
    const pratt::result<std::string> parsed =
        pratt::parse(table{}, sexpr::stream(source), max_depth);
    require(BOOST_TEST(parsed.has_error()));
    BOOST_TEST(parsed.error() == pratt::error::nesting_too_deep);
}

void a_prefix_operator_costs_one_level() {
    parses_within("---a", 4, "(neg (neg (neg a)))");
    too_deep_within("----a", 4);
}

void a_right_associative_chain_costs_one_level_per_operator() {
    parses_within("a^b^c^d", 4, "(^ a (^ b (^ c d)))");
    too_deep_within("a^b^c^d^e", 4);
}

void a_left_associative_chain_costs_one_level_for_its_operands() {
    parses_within("a-b-c-d-e-f", 2, "(- (- (- (- (- a b) c) d) e) f)");
    too_deep_within("a-b", 1);
}

void a_postfix_operator_costs_no_level() {
    parses_within("a!!!", 1, "(! (! (! a)))");
}

void the_refused_call_reads_no_token() {
    const table grammar;
    const std::string source = nested(4);
    table_parser parser(grammar, sexpr::stream(source), 4);
    const pratt::result<std::string> deep = parser.expression(0);
    require(BOOST_TEST(deep.has_error()));
    BOOST_TEST(deep.error() == pratt::error::nesting_too_deep);
    BOOST_TEST_EQ(parser.peek(), 'a');
}

void a_parser_is_reusable_after_an_error() {
    const table grammar;
    const std::string source = nested(4);
    table_parser parser(grammar, sexpr::stream(source), 4);
    const pratt::result<std::string> deep = parser.expression(0);
    require(BOOST_TEST(deep.has_error()));
    BOOST_TEST(deep.error() == pratt::error::nesting_too_deep);

    const pratt::result<std::string> next = parser.expression(0);
    require(BOOST_TEST(next.has_value()));
    BOOST_TEST_EQ(*next, "a");
}

void a_parser_is_reusable_after_an_error_in_an_operand() {
    const table grammar;
    table_parser parser(grammar, sexpr::stream("a+)a"), 2);
    const pratt::result<std::string> failed = parser.expression(0);
    require(BOOST_TEST(failed.has_error()));
    BOOST_TEST(failed.error() == pratt::error::unexpected_token);

    const pratt::result<std::string> next = parser.expression(0);
    require(BOOST_TEST(next.has_value()));
    BOOST_TEST_EQ(*next, "a");
}

void a_parser_is_reusable_after_a_success() {
    const table grammar;
    const std::string source = nested(3) + nested(3);
    table_parser parser(grammar, sexpr::stream(source), 4);
    const pratt::result<std::string> first = parser.expression(0);
    require(BOOST_TEST(first.has_value()));
    BOOST_TEST_EQ(*first, "a");

    const pratt::result<std::string> second = parser.expression(0);
    require(BOOST_TEST(second.has_value()));
    BOOST_TEST_EQ(*second, "a");
}

void parse_bounds_the_depth_by_default() {
    const pratt::result<std::string> within =
        pratt::parse(table{}, sexpr::stream(nested(pratt::default_max_depth - 1)));
    require(BOOST_TEST(within.has_value()));
    BOOST_TEST_EQ(*within, "a");

    const pratt::result<std::string> deep =
        pratt::parse(table{}, sexpr::stream(nested(pratt::default_max_depth)));
    require(BOOST_TEST(deep.has_error()));
    BOOST_TEST(deep.error() == pratt::error::nesting_too_deep);
}

/** The engine's three errors reach a grammar whose node converts from anything, an error too. */
void the_engine_errors_reach_a_node_that_converts_from_anything() {
    BOOST_TEST(pratt::parse(accepting{}, sexpr::stream("(a)")).has_value());
    const auto unclosed = pratt::parse(accepting{}, sexpr::stream("(a"));
    BOOST_TEST(unclosed.has_error() && unclosed.error() == pratt::error::expected_closing_token);
    const auto trailing = pratt::parse(accepting{}, sexpr::stream("a)"));
    BOOST_TEST(trailing.has_error() && trailing.error() == pratt::error::trailing_input);
    const auto deep = pratt::parse(accepting{}, sexpr::stream(nested(4)), 4);
    BOOST_TEST(deep.has_error() && deep.error() == pratt::error::nesting_too_deep);
}

}  // namespace

int main() {
    a_tighter_operator_takes_the_operand_between();
    an_infix_left_operator_associates_to_the_left();
    an_infix_right_operator_associates_to_the_right();
    a_prefix_operator_binds_with_its_own_power();
    a_postfix_operator_takes_everything_that_binds_tighter();
    a_grouping_needs_its_closing_token();
    the_engine_and_the_grammar_report_errors();
    expression_dispatches_to_nud_and_led_only();
    the_depth_is_bounded_by_max_depth();
    a_prefix_operator_costs_one_level();
    a_right_associative_chain_costs_one_level_per_operator();
    a_left_associative_chain_costs_one_level_for_its_operands();
    a_postfix_operator_costs_no_level();
    the_refused_call_reads_no_token();
    a_parser_is_reusable_after_an_error();
    a_parser_is_reusable_after_an_error_in_an_operand();
    a_parser_is_reusable_after_a_success();
    parse_bounds_the_depth_by_default();
    the_engine_errors_reach_a_node_that_converts_from_anything();
    return boost::report_errors();
}
