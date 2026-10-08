// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The calculator in its default number type, boost::decimal::decimal64_t: values, variables kept
// in one environment from line to line, and errors.

// tag::includes[]
#include <webcpp/pratt/calculator.hpp>

#include <boost/decimal.hpp>
#include <boost/decimal/charconv.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;
// end::includes[]

namespace {

// tag::print[]
/** Writes `line`, then its value in Boost.Decimal's shortest form, or its error. */
void print(std::string_view line, const pratt::result<boost::decimal::decimal64_t>& value) {
    std::cout << line << " -> ";
    if (value.has_error()) {
        std::cout << "error: " << value.error().message() << '\n';
        return;
    }
    std::array<char, 48> buffer{};
    const boost::decimal::to_chars_result written =
        boost::decimal::to_chars(buffer.data(), buffer.data() + buffer.size(), *value);
    std::cout << std::string_view(buffer.data(), written.ptr) << '\n';
}

// end::print[]

}  // namespace

int main() {
    // tag::values[]
    calc::environment<> env;
    for (const std::string_view line : {
             "43 + 12 + 2 * (44 - 12) ^ 4",
             "0.1 + 0.2",
             "0.1 + 0.2 == 0.3",
             "2 ^ -3",
             "-2 ^ 2",
             "2 ^ 3 ^ 2",
             "7 % 3",
             "1 / 3",
         }) {
        print(line, calc::evaluate(line, env));
    }
    // end::values[]

    // tag::variables[]
    for (const std::string_view line : {
             "price = 19.99",
             "quantity = 3",
             "price * quantity",
             "quantity++",
             "quantity",
             "--quantity",
             "a = b = 2",
             "a + b",
         }) {
        print(line, calc::evaluate(line, env));
    }
    // end::variables[]

    // tag::errors[]
    for (const std::string_view line : {
             "1 / 0",
             "5 % 0",
             "total + 1",
             "5 = 3",
             "3++",
             "(1 + 2",
             "1 2",
             "1 +",
             "1 $ 2",
         }) {
        print(line, calc::evaluate(line, env));
    }
    // end::errors[]

    // tag::writes[]
    print("i = 1", calc::evaluate("i = 1", env));
    print("i++ $", calc::evaluate("i++ $", env));
    print("i", calc::evaluate("i", env));
    print("x + 1 $", calc::evaluate("x + 1 $", env));
    // end::writes[]
}
