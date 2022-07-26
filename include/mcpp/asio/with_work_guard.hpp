// Copyright Mika Fischer 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <mcpp/asio/async_op_utils.hpp>
#include <mcpp/asio/config.hpp>

#if MCPP_ASIO_USE_BOOST
#include <boost/asio/executor_work_guard.hpp>
#else
#include <asio/executor_work_guard.hpp>
#endif

#include <tuple>
#include <type_traits>

namespace mcpp::asio::detail {

template <typename E>
using work_guard = ::MCPP_ASIO_NAMESPACE::executor_work_guard<E>;

template <typename... Executor>
struct with_work_guard_impl {
    std::tuple<work_guard<Executor>...> work_guards_;

    explicit with_work_guard_impl(work_guard<Executor> &&...work_guards) : work_guards_(std::move(work_guards)...) {}

    struct handler_impl {
        std::tuple<work_guard<Executor>...> work_guards_;

        explicit handler_impl(with_work_guard_impl &&token_impl) : work_guards_(std::move(token_impl.work_guards_)) {}
    };
};

} // namespace mcpp::asio::detail

namespace mcpp::asio {
template <typename CT, typename... E>
inline auto with_work_guard(CT &&token, const E &...executors) {
    return detail::make_wrapped_token<detail::with_work_guard_impl<E...>>(std::forward<CT>(token),
                                                                          detail::work_guard<E>(executors)...);
}
} // namespace mcpp::asio
