// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// calc::environment with an allocator of the program's own, which counts what the environment
// allocates: a new name allocates, and an evaluation, a read or a replaced value allocates nothing.
// Only what each line adds is printed, since how many allocations a map makes is its standard
// library's own.

#include <webcpp/pratt/calculator.hpp>

#include <boost/decimal.hpp>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string_view>

namespace calc = webcpp::pratt::calc;

namespace {

// tag::allocator[]
/** What a counting allocator has done, in an object its program owns. */
struct tally {
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
};

/** An allocator that counts into a tally, and takes its memory from std::allocator. */
template <class T>
class counting {
public:
    using value_type = T;

    explicit counting(tally* count) noexcept : count_(count) {}

    template <class U>
    explicit counting(const counting<U>& other) noexcept : count_(other.count()) {}

    T* allocate(std::size_t n) {
        ++count_->allocations;
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* pointer, std::size_t n) noexcept {
        ++count_->deallocations;
        std::allocator<T>{}.deallocate(pointer, n);
    }

    [[nodiscard]] tally* count() const noexcept { return count_; }

    template <class U>
    bool operator==(const counting<U>& other) const noexcept {
        return count_ == other.count();
    }

private:
    tally* count_;
};

// end::allocator[]

}  // namespace

int main() {
    // tag::use[]
    tally count;
    {
        calc::environment<boost::decimal::decimal64_t, counting<char>> env{counting<char>(&count)};
        for (const std::string_view line : {"x = 6", "y = 7", "x * y + 1", "x = 8", "x * y"}) {
            const std::size_t before = count.allocations;
            const bool evaluated = calc::evaluate(line, env).has_value();
            std::cout << line << (evaluated ? "" : " failed") << " -> "
                      << (count.allocations > before ? "allocated" : "allocated nothing") << '\n';
        }
    }
    std::cout << "environment destroyed -> deallocations == allocations: "
              << (count.deallocations == count.allocations ? "yes" : "no") << '\n';
    // end::use[]
}
