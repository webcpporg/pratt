// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The depth limit: a grouping nested as deep as default_max_depth allows is evaluated, one level
// more is refused before the stack grows, and a larger max_depth lets it through.

#include <webcpp/pratt/calculator.hpp>

#include <boost/decimal.hpp>

#include <iostream>
#include <string>

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;

namespace {

/** `1` inside `levels` pairs of parentheses, `levels + 1` expression calls deep. */
std::string nested(int levels) {
    const auto count = static_cast<std::string::size_type>(levels);
    return std::string(count, '(') + "1" + std::string(count, ')');
}

/** Writes what `value` holds: 1, the value every nesting of `1` has, or its error. */
void print(const pratt::result<boost::decimal::decimal64_t>& value) {
    if (value.has_error()) {
        std::cout << "error: " << value.error().message() << '\n';
        return;
    }
    std::cout << (*value == 1 ? "1" : "another value") << '\n';
}

}  // namespace

int main() {
    // tag::depth[]
    calc::environment<> env;
    std::cout << "default_max_depth: " << pratt::default_max_depth << '\n';

    std::cout << "63 pairs: ";
    print(calc::evaluate(nested(63), env));

    std::cout << "64 pairs: ";
    print(calc::evaluate(nested(64), env));

    std::cout << "64 pairs, max_depth 128: ";
    print(calc::evaluate(nested(64), env, 128));

    std::cout << "10000 opening parentheses: ";
    print(calc::evaluate(std::string(10000, '('), env));
    // end::depth[]
}
