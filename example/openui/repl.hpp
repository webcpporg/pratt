// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The REPL of the openui-lang micro-subset, shared by the examples that run
 it on different inputs.
*/
#ifndef WEBCPP_EXAMPLE_PRATT_OPENUI_REPL_HPP
#define WEBCPP_EXAMPLE_PRATT_OPENUI_REPL_HPP

#include <webcpp/pratt/parser.hpp>

#include <istream>
#include <ostream>
#include <string>

#include "grammar.hpp"

namespace webcpp::example::openui {

/**
 Parses each line of `in` and writes its S-expression to `out`, or `error: `
 and the error's message.

 A line's one trailing carriage return is dropped, so a line that ends in a
 carriage return and a line feed reads as one that ends in a line feed. A
 line that is empty or holds only spaces is skipped. The line `exit`,
 exactly, ends the session; `exit ` with a space is parsed as any other
 line, the name `exit`.

 @param in The lines.
 @param out Where the results go.
 @return 0, the program's status, at `exit` or at the end of `in`.
*/
inline int repl(std::istream& in, std::ostream& out) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "exit") {
            return 0;
        }
        if (line.find_first_not_of(' ') == std::string::npos) {
            continue;
        }
        const pratt::result<std::string> tree = pratt::parse(grammar{}, stream(line));
        if (tree.has_error()) {
            out << "error: " << tree.error().message() << '\n';
            continue;
        }
        out << *tree << '\n';
    }
    return 0;
}

}  // namespace webcpp::example::openui

#endif  // WEBCPP_EXAMPLE_PRATT_OPENUI_REPL_HPP
