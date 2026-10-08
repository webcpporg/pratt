// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/pratt/errors.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>

namespace pratt = webcpp::pratt;

namespace {

void values_are_fixed() {
    BOOST_TEST_EQ(static_cast<int>(pratt::error::unexpected_token), 1);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::unexpected_end), 2);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::expected_closing_token), 3);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::trailing_input), 4);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::invalid_assignment_target), 5);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::undefined_variable), 6);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::invalid_character), 7);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::nesting_too_deep), 8);
    BOOST_TEST_EQ(static_cast<int>(pratt::error::division_by_zero), 9);
}

void the_category_is_webcpp_pratt() {
    BOOST_TEST_EQ(std::string(pratt::error_category().name()), "webcpp.pratt");

    const boost::system::error_code code = pratt::error::nesting_too_deep;
    BOOST_TEST(code.category() == pratt::error_category());
    BOOST_TEST_EQ(code.message(), "expression nested too deeply");

    BOOST_TEST_EQ(boost::system::error_code(pratt::error::unexpected_token).message(),
                  "unexpected token");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::unexpected_end).message(),
                  "unexpected end of input");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::expected_closing_token).message(),
                  "expected closing token");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::trailing_input).message(),
                  "trailing input after expression");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::invalid_assignment_target).message(),
                  "invalid assignment target");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::undefined_variable).message(),
                  "undefined variable");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::invalid_character).message(),
                  "invalid character");
    BOOST_TEST_EQ(boost::system::error_code(pratt::error::division_by_zero).message(),
                  "division by zero");

    BOOST_TEST_EQ(pratt::error_category().message(0), "unknown webcpp.pratt error");
    BOOST_TEST_EQ(pratt::error_category().message(10), "unknown webcpp.pratt error");
}

void a_result_holds_an_error() {
    const pratt::result<int> r = pratt::error::unexpected_end;
    BOOST_TEST(r.has_error());
    BOOST_TEST(r.error() == pratt::error::unexpected_end);
}

}  // namespace

int main() {
    values_are_fixed();
    the_category_is_webcpp_pratt();
    a_result_holds_an_error();
    return boost::report_errors();
}
