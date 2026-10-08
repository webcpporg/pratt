// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A REPL of the calculator: each line of the standard input is evaluated in one session's
// environment, and its value is printed in Boost.Decimal's shortest form, or `error: ` and the
// error's message. The line `exit` or the end of the input ends it.

#include <webcpp/pratt/calculator.hpp>

#include <boost/decimal/charconv.hpp>

#include <array>
#include <iostream>
#include <ostream>
#include <string_view>
#include <system_error>

#include "repl.hpp"

namespace {

namespace calc = webcpp::pratt::calc;

/**
 Writes `value` to `out` in Boost.Decimal's shortest round-tripping form:
 `2097207`, `1048503.5`, with its sign for a negative value, or `error: `
 and the reason when it does not fit the buffer.
*/
void print(const calc::environment<>::number_type& value, std::ostream& out) {
    std::array<char, 48> buffer{};
    const boost::decimal::to_chars_result written =
        boost::decimal::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (written.ec != std::errc{}) {
        out << "error: " << std::make_error_code(written.ec).message();
        return;
    }
    out.write(buffer.data(), written.ptr - buffer.data());
}

}  // namespace

// tag::main[]
int main() {
    calc::environment<> env;
    return webcpp::example::repl(
        std::cin, std::cout, [&env](std::string_view line, std::ostream& out) {
            const webcpp::pratt::result<calc::environment<>::number_type> value =
                calc::evaluate(line, env);
            if (value.has_error()) {
                out << "error: " << value.error().message() << '\n';
                return;
            }
            print(*value, out);
            out << '\n';
        });
}

// end::main[]
