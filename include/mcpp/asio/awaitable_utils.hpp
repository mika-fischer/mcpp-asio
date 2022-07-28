// Copyright Mika Fischer 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "mcpp/asio/awaitable_utils.hpp"
#include <exception>
#include <mcpp/asio/config.hpp>

#if MCPP_ASIO_USE_BOOST
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/use_awaitable.hpp>
#else
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/deferred.hpp>
#include <asio/experimental/parallel_group.hpp>
#include <asio/use_awaitable.hpp>
#endif

#include <iostream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace mcpp::asio {
namespace detail {

template <typename T>
struct to_variant_type {
    using type = T;
};

template <>
struct to_variant_type<void> {
    using type = std::monostate;
};

template <typename T>
using to_variant_type_t = typename to_variant_type<T>::type;

template <size_t SIZE, typename F, size_t N = 0>
auto invoke_with_idx(size_t i, F &&f) {
    if (N == i) {
        return std::invoke(std::forward<F>(f), std::integral_constant<size_t, N>{});
    }
    if constexpr (N + 1 >= SIZE) {
        throw std::out_of_range("Index out of range!");
    } else {
        return invoke_with_idx<SIZE, F, N + 1>(i, std::forward<F>(f));
    }
}

template <typename... Ts>
struct awaitable_traits {
    template <size_t I>
    using type_at_t = std::tuple_element_t<I, std::tuple<Ts...>>;

    template <typename T>
    static constexpr auto num_result_elements() -> size_t {
        if constexpr (std::is_same_v<T, void>) {
            return 1;
        } else {
            return 2;
        }
    }

    template <size_t... Is>
    static constexpr auto group_results_idx_for_impl(std::index_sequence<Is...> /*unused*/) {
        if constexpr (sizeof...(Is) > 0) {
            return 1 + (num_result_elements<type_at_t<Is>>() + ...);
        } else {
            return 1;
        }
    }

    template <size_t I>
    static inline constexpr auto group_result_idx_for_v = group_results_idx_for_impl(std::make_index_sequence<I>());

    template <size_t I>
    static constexpr auto get_group_result(auto &group_result) -> to_variant_type_t<type_at_t<I>> {
        constexpr auto result_idx = group_result_idx_for_v<I>;
        if constexpr (std::is_same_v<type_at_t<I>, void>) {
            return std::monostate{};
        } else {
            return std::move(std::get<result_idx + 1>(group_result));
        }
    }

    template <size_t I>
    static constexpr auto get_group_result_or_throw(auto &group_result) -> to_variant_type_t<type_at_t<I>> {
        constexpr auto result_idx = group_result_idx_for_v<I>;
        if (auto error = std::get<result_idx>(group_result)) {
            std::rethrow_exception(error);
        }
        return get_group_result<I>(group_result);
    }

    template <size_t I>
    static constexpr auto get_group_result_or_error(auto &group_result)
        -> std::variant<to_variant_type_t<type_at_t<I>>, std::exception_ptr> {
        using return_type = std::variant<to_variant_type_t<type_at_t<I>>, std::exception_ptr>;
        constexpr auto result_idx = group_result_idx_for_v<I>;
        if (auto error = std::get<result_idx>(group_result)) {
            return return_type(std::in_place_index<1>, error);
        }
        if constexpr (std::is_same_v<type_at_t<I>, void>) {
            return return_type(std::in_place_index<0>);
        } else {
            return return_type(std::in_place_index<0>, std::move(std::get<result_idx + 1>(group_result)));
        }
    }
};

template <typename T, typename E>
using awaitable = ::MCPP_ASIO_NAMESPACE::awaitable<T, E>;

} // namespace detail

// First exception or result wins.
// All other awaitables are canceled and their results/errors ignored
template <typename E, typename... Ts>
auto race(detail::awaitable<Ts, E> &&...awaitables)
    -> detail::awaitable<std::variant<detail::to_variant_type_t<Ts>...>, E> {
    using ::MCPP_ASIO_NAMESPACE::deferred, ::MCPP_ASIO_NAMESPACE::use_awaitable,
        ::MCPP_ASIO_NAMESPACE::experimental::make_parallel_group, ::MCPP_ASIO_NAMESPACE::experimental::wait_for_one;
    namespace this_coro = ::MCPP_ASIO_NAMESPACE::this_coro;
    using traits = detail::awaitable_traits<Ts...>;

    auto group = make_parallel_group(co_spawn(co_await this_coro::executor, std::move(awaitables), deferred)...);
    auto results = co_await group.async_wait(wait_for_one(), use_awaitable);
    auto awaitable_idx = std::get<0>(results)[0];
    co_return detail::invoke_with_idx<sizeof...(Ts)>(awaitable_idx, [&results](auto I) {
        return std::variant<detail::to_variant_type_t<Ts>...>(std::in_place_index<I>,
                                                              traits::template get_group_result_or_throw<I>(results));
    });
}

// First exception wins.
// All other awaitables are canceled and their results/errors ignored.
// Otherwise all results are returned as a tuple.
template <typename E, typename... Ts>
auto all(detail::awaitable<Ts, E> &&...awaitables)
    -> detail::awaitable<std::tuple<detail::to_variant_type_t<Ts>...>, E> {
    using ::MCPP_ASIO_NAMESPACE::deferred, ::MCPP_ASIO_NAMESPACE::use_awaitable,
        ::MCPP_ASIO_NAMESPACE::experimental::make_parallel_group,
        ::MCPP_ASIO_NAMESPACE::experimental::wait_for_one_error;
    namespace this_coro = ::MCPP_ASIO_NAMESPACE::this_coro;
    using traits = detail::awaitable_traits<Ts...>;

    auto group = make_parallel_group(co_spawn(co_await this_coro::executor, std::move(awaitables), deferred)...);
    auto results = co_await group.async_wait(wait_for_one_error(), use_awaitable);
    for (auto awaitable_idx : std::get<0>(results)) {
        if (auto error = detail::invoke_with_idx<sizeof...(Ts)>(awaitable_idx, [&results](auto I) {
                return std::get<traits::template group_result_idx_for_v<I>>(results);
            })) {
            std::rethrow_exception(error);
        }
    }
    co_return [&results]<size_t... Is>(std::index_sequence<Is...>) {
        return std::tuple(traits::template get_group_result<Is>(results)...);
    }
    (std::index_sequence_for<Ts...>{});
}

// Waits until all operations are complete, nothing is ever cancelled, except if the returned awaitable is canceled.
template <typename E, typename... Ts>
auto all_settled(detail::awaitable<Ts, E> &&...awaitables)
    -> detail::awaitable<std::tuple<std::variant<detail::to_variant_type_t<Ts>, std::exception_ptr>...>, E> {
    using ::MCPP_ASIO_NAMESPACE::deferred, ::MCPP_ASIO_NAMESPACE::use_awaitable,
        ::MCPP_ASIO_NAMESPACE::experimental::make_parallel_group, ::MCPP_ASIO_NAMESPACE::experimental::wait_for_all;
    namespace this_coro = ::MCPP_ASIO_NAMESPACE::this_coro;
    using traits = detail::awaitable_traits<Ts...>;

    auto group = make_parallel_group(co_spawn(co_await this_coro::executor, std::move(awaitables), deferred)...);
    auto results = co_await group.async_wait(wait_for_all(), use_awaitable);
    co_return [&results]<size_t... Is>(std::index_sequence<Is...> is) {
        return std::tuple(traits::template get_group_result_or_error<Is>(results)...);
    }
    (std::index_sequence_for<Ts...>{});
}

} // namespace mcpp::asio