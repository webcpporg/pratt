// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// An environment of the program's own: two variables in fixed storage, which allocates nothing,
// and refuses a third name with an error of its own, which the calculator returns unchanged.

#include <webcpp/pratt/calculator.hpp>

#include <boost/system/errc.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace pratt = webcpp::pratt;
namespace calc = webcpp::pratt::calc;

namespace {

// tag::environment[]
/** Two variables of names up to 8 characters, in fixed storage, computed in double. */
class two_variables {
public:
    using number_type = double;

    [[nodiscard]] const double* lookup(std::string_view name) const {
        for (std::size_t i = 0; i < used_; ++i) {
            if (slots_[i].name() == name) {
                return &slots_[i].value;
            }
        }
        return nullptr;
    }

    pratt::result<void> assign(std::string_view name, double value) {
        for (std::size_t i = 0; i < used_; ++i) {
            if (slots_[i].name() == name) {
                slots_[i].value = value;
                return {};
            }
        }
        if (used_ == slots_.size() || name.size() > slot::capacity) {
            return boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
        }
        slot& fresh = slots_[used_++];
        std::ranges::copy(name, fresh.characters.begin());
        fresh.length = name.size();
        fresh.value = value;
        return {};
    }

private:
    struct slot {
        static constexpr std::size_t capacity = 8;

        std::array<char, capacity> characters{};
        std::size_t length = 0;
        double value = 0;

        [[nodiscard]] std::string_view name() const { return {characters.data(), length}; }
    };

    std::array<slot, 2> slots_{};
    std::size_t used_ = 0;
};

static_assert(calc::environment_for<two_variables>);
// end::environment[]

}  // namespace

int main() {
    // tag::use[]
    two_variables env;
    for (const std::string_view line : {
             "width = 3",
             "height = 4",
             "width * height",
             "width = 5",
             "width * height",
             "depth = 2",
             "depth",
         }) {
        const pratt::result<double> value = calc::evaluate(line, env);
        std::cout << line << " -> ";
        if (value.has_value()) {
            std::cout << *value << '\n';
        } else if (value.error() == boost::system::errc::not_enough_memory) {
            std::cout << "refused: the environment is full\n";
        } else {
            std::cout << "error: " << value.error().message() << '\n';
        }
    }
    // end::use[]
}
