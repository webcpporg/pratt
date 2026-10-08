// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A REPL of the openui-lang micro-subset: each line of the standard input is parsed and printed
// back as the S-expression of its tree, or as `error: ` and the error's message. The line `exit`
// or the end of the input ends it.

#include <iostream>

#include "openui/repl.hpp"

// tag::main[]
int main() {
    return webcpp::example::openui::repl(std::cin, std::cout);
}

// end::main[]
