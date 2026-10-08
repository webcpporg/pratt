// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The calculator computing in double, an environment's number type: integral powers stay exact,
// and the decimal fractions are binary ones, as double makes them.

#include <webcpp/pratt/calculator.hpp>

#include <array>
#include <charconv>
#include <iostream>
#include <string_view>

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;

namespace {

/** Writes `line`, then its value in the shortest form that reads back as the same double. */
void print(std::string_view line, const pratt::result<double>& value) {
    std::cout << line << " -> ";
    if (value.has_error()) {
        std::cout << "error: " << value.error().message() << '\n';
        return;
    }
    std::array<char, 32> buffer{};
    const std::to_chars_result written =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), *value);
    std::cout << std::string_view(buffer.data(), written.ptr) << '\n';
}

}  // namespace

int main() {
    // tag::double[]
    calc::environment<double> env;
    for (const std::string_view line : {
             "43 + 12 + 2 * (44 - 12) ^ 4",
             "0.1 + 0.2",
             "0.1 + 0.2 == 0.3",
             "2 ^ -3",
             "2 ^ 60",
             "2.25 ^ 0.5",
             "1 / 0",
         }) {
        print(line, calc::evaluate(line, env));
    }
    // end::double[]
}
