// Copyright (C) mcpp::asio Intelligente Videoanalyse GmbH - All rights reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited.
// Proprietary and confidential.

#pragma once

#include <utils/asio/transform_noexcept.hpp>

#include <exception>
#include <type_traits>
#include <utils/asio/async_op_utils.hpp>

namespace mcpp::asio::detail {

template <typename Signature>
struct transform_noexcept_signature;

#define X(ref_qualifier, noexcept_qualifier)                                                                           \
    template <typename R, typename... Args>                                                                            \
    struct transform_noexcept_signature<R(std::exception_ptr, Args...) ref_qualifier noexcept_qualifier> {             \
        using type = R(Args...) ref_qualifier noexcept_qualifier;                                                      \
    };                                                                                                                 \
    template <typename R, typename... Args>                                                                            \
    struct transform_noexcept_signature<R(Args...) ref_qualifier noexcept_qualifier> {                                 \
        using type = R(Args...) ref_qualifier noexcept_qualifier;                                                      \
    };
mcpp::asio_FOREACH_SIGNATURE_QUALIFIER(X)
#undef X

    struct transform_noexcept_impl {
    template <typename Signature>
    using transform_signature = typename transform_noexcept_signature<Signature>::type;

    template <typename Handler>
    struct handler_impl : wrapped_handler_impl_base<Handler> {
        using wrapped_handler_impl_base<Handler>::wrapped_handler_impl_base;

        template <typename... Args>
        void operator()(std::exception_ptr error, Args &&...args) && {
            if (error) {
                try {
                    std::rethrow_exception(error);
                } catch (...) {
                    std::terminate();
                }
            }
            return std::move(*this).complete(std::forward<Args>(args)...);
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
inline auto transform_noexcept(CompletionToken &&token) {
    return detail::wrapped_token<detail::transform_noexcept_impl, std::decay_t<CompletionToken>>(
        std::forward<CompletionToken>(token));
}
} // namespace mcpp::asio
