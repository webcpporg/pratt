// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests what the calculator allocates, by counting every call of the global operator new this
// program makes: nothing over an environment in fixed storage, and over calc::environment only
// what its allocator, the user's, allocates.

#include <webcpp/pratt/calculator.hpp>

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/decimal.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string_view>

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include "environments.hpp"

namespace calc = webcpp::pratt::calc;

using boost::decimal::decimal64_t;
using webcpp::test::allocation_count;
using webcpp::test::counting;
using webcpp::test::fixed_environment;

namespace {

// The calls of the global operator new this program has made. It is mutable global state, which
// no library holds: it is this test program's own instrument, the one place where the replaced
// operator new below can record what it sees.
std::size_t global_allocations = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

// The replacements of the global operator new and operator delete, for this program alone, which
// count every allocation, an over-aligned one included (a Number whose alignment is above
// __STDCPP_DEFAULT_NEW_ALIGNMENT__ is allocated through the align_val_t forms). The standard
// library declares them with parameter names reserved to the implementation, which a program may
// not use, hence the exemption on each. None is ever inlined: GCC, seeing the malloc or the free
// inside one, would pair it with the other's call and warn of a mismatch (-Wmismatched-new-delete).

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size) {
    ++global_allocations;
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        std::abort();
    }
    return memory;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory) noexcept {
    std::free(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/) noexcept {
    std::free(memory);
}

namespace {

/**
 Memory of `size` bytes aligned to `alignment`: from aligned_alloc, whose size must be a multiple
 of the alignment; or, under _MSC_VER, from _aligned_malloc, which needs no such multiple but
 takes the same rounded size regardless.

 Tip: the UCRT of MSVC has no aligned_alloc, and memory from its _aligned_malloc is given back with
 _aligned_free, never free, hence the two below.
*/
void* aligned_memory(std::size_t size, std::size_t alignment) {
    const std::size_t rounded =
        size == 0 ? alignment : (size + alignment - 1) / alignment * alignment;
#ifdef _MSC_VER
    return _aligned_malloc(rounded, alignment);
#else
    return std::aligned_alloc(alignment, rounded);
#endif
}

/** Gives back what aligned_memory took. */
void free_aligned_memory(void* memory) noexcept {
#ifdef _MSC_VER
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

}  // namespace

/** The aligned allocation, from aligned_memory. */
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size, std::align_val_t alignment) {
    ++global_allocations;
    void* memory = aligned_memory(size, static_cast<std::size_t>(alignment));
    if (memory == nullptr) {
        std::abort();
    }
    return memory;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::align_val_t /*alignment*/) noexcept {
    free_aligned_memory(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/,
                                    std::align_val_t /*alignment*/) noexcept {
    free_aligned_memory(memory);
}

namespace {

/**
 Twenty lines of the calculator: assignments, reads, increments, every kind of power, and errors.

 Tip: the names are longer than any small-string buffer, so that a key of calc::environment
 allocates; the last line writes a third name, which the fixed environment refuses.
*/
constexpr std::array<std::string_view, 20> lines{
    "first_name_past_any_sso_buffer = 43 + 12 + 2 * (44 - 12) ^ 4",
    "second_name_past_any_sso_buffer = first_name_past_any_sso_buffer / 2 - 100",
    "first_name_past_any_sso_buffer",
    "second_name_past_any_sso_buffer > 1000000",
    "2 ^ 3 ^ 2",
    "-2 ^ 2",
    "10 % 3",
    "first_name_past_any_sso_buffer = 0",
    "first_name_past_any_sso_buffer++",
    "++first_name_past_any_sso_buffer",
    "--first_name_past_any_sso_buffer",
    "first_name_past_any_sso_buffer--",
    "(first_name_past_any_sso_buffer) = 1.5 * 2",
    "2 ^ -3",
    "2 ^ 0.5",
    "0.123456789012345 * 1",
    "1 / 0",
    "undefined_name_past_any_sso_buffer + 1",
    "1 $ 2",
    "third_name_past_any_sso_buffer = 1",
};

/** Evaluates every line over `env` and returns how many gave a value. */
template <class Environment>
std::size_t evaluate_every_line(Environment& env) {
    std::size_t values = 0;
    for (const std::string_view line : lines) {
        if (calc::evaluate(line, env).has_value()) {
            ++values;
        }
    }
    return values;
}

void a_fixed_environment_allocates_nothing() {
    const std::size_t before = global_allocations;
    fixed_environment<decimal64_t> env;
    const std::size_t values = evaluate_every_line(env);
    const std::size_t after = global_allocations;
    BOOST_TEST_EQ(values, 16U);
    BOOST_TEST_EQ(after - before, 0U);
}

void the_environment_allocates_only_through_its_allocator() {
    allocation_count count;
    const std::size_t before = global_allocations;
    {
        calc::environment<decimal64_t, counting<char>> env{counting<char>(&count)};
        const std::size_t values = evaluate_every_line(env);
        BOOST_TEST_EQ(values, 17U);
    }
    const std::size_t after = global_allocations;
    BOOST_TEST_GT(count.allocations, 0U);
    BOOST_TEST_EQ(after - before, count.allocations);
    BOOST_TEST_EQ(count.deallocations, count.allocations);
}

/** A type whose alignment is above what the plain operator new guarantees. */
struct alignas(2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__) over_aligned {
    std::array<char, 2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__> bytes{};
};

/**
 An over-aligned allocation goes through the replaced aligned operator new, and is counted.

 Tip: the operator is called as a function, which a compiler may not elide; GCC elides an
 allocation through std::allocator, or a new-expression, whose memory does not escape.
*/
void an_over_aligned_allocation_is_counted() {
    constexpr std::align_val_t alignment{alignof(over_aligned)};
    const std::size_t before = global_allocations;
    void* memory = ::operator new(3 * sizeof(over_aligned), alignment);
    const std::size_t after = global_allocations;
    BOOST_TEST_EQ(after - before, 1U);
    BOOST_TEST_EQ(reinterpret_cast<std::uintptr_t>(memory) % alignof(over_aligned), 0U);
    ::operator delete(memory, alignment);
}

}  // namespace

int main() {
    an_over_aligned_allocation_is_counted();
    a_fixed_environment_allocates_nothing();
    the_environment_allocates_only_through_its_allocator();
    return boost::report_errors();
}
