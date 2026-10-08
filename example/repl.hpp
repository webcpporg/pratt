// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The line-reading loop the pratt examples' REPLs share.
*/
#ifndef WEBCPP_EXAMPLE_PRATT_REPL_HPP
#define WEBCPP_EXAMPLE_PRATT_REPL_HPP

#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace webcpp::example {

/**
 Reads lines from `in`, handing every non-blank line but `exit` to
 `process_line`, until the line `exit`, exactly, or the end of `in`.

 A line's one trailing carriage return is dropped, so a line that ends in a
 carriage return and a line feed reads as one that ends in a line feed. A
 line that is empty or holds only spaces or tabs is skipped, without calling
 `process_line`. `exit ` with a trailing space is an ordinary line, the name
 `exit`, not the one that ends the session; nothing after the line `exit` is
 read.

 @tparam ProcessLine A callable `void(std::string_view, std::ostream&)`.
 @param in The lines.
 @param out Where a line's output goes.
 @param process_line What a non-blank line does with its text and `out`.
 @return 0, the program's status, at `exit` or at the end of `in`.
*/
// tag::loop[]
template <class ProcessLine>
int repl(std::istream& in, std::ostream& out, ProcessLine process_line) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "exit") {
            return 0;
        }
        if (line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }
        process_line(std::string_view(line), out);
    }
    return 0;
}

// end::loop[]

}  // namespace webcpp::example

#endif  // WEBCPP_EXAMPLE_PRATT_REPL_HPP
