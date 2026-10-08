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
#include <string_view>

#include "../repl.hpp"
#include "grammar.hpp"

namespace webcpp::example::openui {

/**
 Parses each line of `in` and writes its S-expression to `out`, or `error: `
 and the error's message, through the line-reading loop of
 @ref webcpp::example::repl.

 @param in The lines.
 @param out Where the results go.
 @return 0, the program's status, at `exit` or at the end of `in`.
*/
inline int repl(std::istream& in, std::ostream& out) {
    return webcpp::example::repl(in, out, [](std::string_view line, std::ostream& line_out) {
        const pratt::result<std::string> tree = pratt::parse(grammar{}, stream(line));
        if (tree.has_error()) {
            line_out << "error: " << tree.error().message() << '\n';
            return;
        }
        line_out << *tree << '\n';
    });
}

}  // namespace webcpp::example::openui

#endif  // WEBCPP_EXAMPLE_PRATT_OPENUI_REPL_HPP
