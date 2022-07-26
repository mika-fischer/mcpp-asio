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

template <typename... Executor>
struct with_work_guard_impl {
    std::tuple<::MCPP_ASIO_NAMESPACE::executor_work_guard<Executor>...> work_guards_;

    explicit with_work_guard_impl(::MCPP_ASIO_NAMESPACE::executor_work_guard<Executor> &&...work_guards)
        : work_guards_(std::move(work_guards)...) {}

    template <typename Signature>
    using transform_signature = Signature;

    template <typename Handler>
    struct handler_impl : wrapped_handler_impl_base<Handler> {
        std::tuple<::MCPP_ASIO_NAMESPACE::executor_work_guard<Executor>...> work_guards_;

        template <typename H>
        handler_impl(H &&handler, with_work_guard_impl &&token_impl)
            : wrapped_handler_impl_base<Handler>(std::forward<H>(handler)),
              work_guards_(std::move(token_impl.work_guards_)) {}
    };
};

} // namespace mcpp::asio::detail

namespace mcpp::asio {
template <typename CompletionToken, typename... Executor>
inline auto with_work_guard(CompletionToken &&token, const Executor &...ex) {
    return detail::make_wrapped_token<detail::with_work_guard_impl<Executor...>>(
        std::forward<CompletionToken>(token), ::MCPP_ASIO_NAMESPACE::executor_work_guard<Executor>(ex)...);
}
} // namespace mcpp::asio
