// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The REPL of openui_repl.cpp on an input whose lines end in "\r\n", whose last line has no
// newline, and which has no `exit`: the REPL reads that last line, then ends with status 0.

#include <iostream>

#include "openui/repl.hpp"

int main() {
    return webcpp::example::openui::repl(std::cin, std::cout);
}
