// Copyright Mika Fischer 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <mcpp/asio/config.hpp>

#include <mcpp/asio/async_op_utils.hpp>

#include <exception>
#include <type_traits>

namespace mcpp::asio::detail {

template <typename Signature>
struct transform_system_error_signature;

#define X(ref_qualifier, noexcept_qualifier)                                                                           \
    template <typename R, typename... Args>                                                                            \
    struct transform_system_error_signature<R(std::exception_ptr, Args...) ref_qualifier noexcept_qualifier> {         \
        using type = R(std::error_code, Args...) ref_qualifier noexcept_qualifier;                                     \
    };                                                                                                                 \
    template <typename R, typename... Args>                                                                            \
    struct transform_system_error_signature<R(Args...) ref_qualifier noexcept_qualifier> {                             \
        using type = R(Args...) ref_qualifier noexcept_qualifier;                                                      \
    };
MCPP_ASIO_FOREACH_SIGNATURE_QUALIFIER(X)
#undef X

struct transform_system_error_impl {
    template <typename Signature>
    using transform_signature = typename transform_system_error_signature<Signature>::type;

    template <typename Handler>
    struct handler_impl : wrapped_handler_impl_base<Handler> {
        using wrapped_handler_impl_base<Handler>::wrapped_handler_impl_base;

        template <typename... Args>
        void operator()(std::exception_ptr error, Args &&...args) && {
            if (!error) {
                return std::move(*this).complete(error_code{}, std::forward<Args>(args)...);
            }
            try {
                std::rethrow_exception(error);
            } catch (system_error &e) {
                return std::move(*this).complete(e.code(), std::forward<Args>(args)...);
            } catch (...) {
                std::terminate();
            }
        }

        template <typename... Args>
        auto operator()(Args &&...args) && {
            return std::move(*this).complete(std::forward<Args>(args)...);
        }
    };
};

} // namespace mcpp::asio::detail

namespace mcpp::asio {
template <typename CompletionToken>
inline auto transform_system_error(CompletionToken &&token) {
    return detail::wrapped_token<detail::transform_system_error_impl, std::decay_t<CompletionToken>>(
        std::forward<CompletionToken>(token));
}
} // namespace mcpp::asio
