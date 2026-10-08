// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The calculator's environments of the tests' own: one in fixed storage, which allocates nothing,
// and an allocator that counts what calc::environment allocates through it. calculator_test and
// allocation_test both use them.

#ifndef WEBCPP_TEST_PRATT_ENVIRONMENTS_HPP
#define WEBCPP_TEST_PRATT_ENVIRONMENTS_HPP

#include <webcpp/pratt/errors.hpp>

#include <boost/system/errc.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <string_view>

namespace webcpp::test {

/**
 An environment of two names in fixed storage, which allocates nothing.

 Tip: a third name, or a name longer than a slot, is refused with errc::not_enough_memory, an error
 of the environment's own that the calculator returns unchanged.
*/
template <class Number>
class fixed_environment {
public:
    using number_type = Number;

    /** The longest name a slot holds. */
    static constexpr std::size_t name_capacity = 64;

    [[nodiscard]] const Number* lookup(std::string_view name) const {
        const std::size_t at = index_of(name);
        if (at == used_) {
            return nullptr;
        }
        return &slots_[at].value;
    }

    webcpp::pratt::result<void> assign(std::string_view name, Number value) {
        const std::size_t at = index_of(name);
        if (at != used_) {
            slots_[at].value = value;
            return {};
        }
        if (used_ == slots_.size() || name.size() > name_capacity) {
            return boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
        }
        slot& fresh = slots_[used_];
        std::copy(name.begin(), name.end(), fresh.name.begin());
        fresh.length = name.size();
        fresh.value = value;
        ++used_;
        return {};
    }

private:
    struct slot {
        std::array<char, name_capacity> name{};
        std::size_t length = 0;
        Number value{0};
    };

    /** The index of the slot holding `name`, or used_ when none does. */
    [[nodiscard]] std::size_t index_of(std::string_view name) const {
        for (std::size_t i = 0; i < used_; ++i) {
            if (std::string_view(slots_[i].name.data(), slots_[i].length) == name) {
                return i;
            }
        }
        return used_;
    }

    std::array<slot, 2> slots_{};
    std::size_t used_ = 0;
};

/** What a counting allocator recorded, in an object its test owns. */
struct allocation_count {
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
};

/**
 An allocator that counts its allocations and deallocations into an allocation_count, and takes
 the memory from the global operator new.

 Tip: the operator is called as a function, which a compiler may not elide, where it may elide
 std::allocator's allocations; allocation_test compares this count with the operator's own.
*/
template <class T>
class counting {
public:
    using value_type = T;

    explicit counting(allocation_count* count) noexcept : count_(count) {}

    template <class U>
    explicit counting(const counting<U>& other) noexcept : count_(other.count()) {}

    T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            std::abort();
        }
        ++count_->allocations;
        if constexpr (over_aligned) {
            return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
        } else {
            return static_cast<T*>(::operator new(n * sizeof(T)));
        }
    }

    void deallocate(T* pointer, std::size_t /*n*/) noexcept {
        ++count_->deallocations;
        if constexpr (over_aligned) {
            ::operator delete(pointer, std::align_val_t{alignof(T)});
        } else {
            ::operator delete(pointer);
        }
    }

    [[nodiscard]] allocation_count* count() const noexcept { return count_; }

    template <class U>
    bool operator==(const counting<U>& other) const noexcept {
        return count_ == other.count();
    }

private:
    /** Whether T needs the aligned operator new, as std::allocator decides. */
    static constexpr bool over_aligned = alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    allocation_count* count_;
};

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_PRATT_ENVIRONMENTS_HPP
