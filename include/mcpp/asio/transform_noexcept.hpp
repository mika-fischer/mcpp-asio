// Copyright Mika Fischer 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <mcpp/asio/async_op_utils.hpp>

#include <exception>
#include <type_traits>

namespace mcpp::asio::detail {

template <typename Signature>
struct transform_noexcept_signature;

#define X(ref_qualifier, noexcept_qualifier)                                                                           \
    template <typename R, typename... Args>                                                                            \
    struct transform_noexcept_signature<R(std::exception_ptr, Args...) ref_qualifier noexcept_qualifier> {             \
        using type = R(Args...) ref_qualifier noexcept_qualifier;                                                      \
    };
MCPP_ASIO_FOREACH_SIGNATURE_QUALIFIER(X)
#undef X

struct transform_noexcept_impl {
    template <typename Signature>
    using transform_signature = typename transform_noexcept_signature<Signature>::type;

    struct handler_impl {
        template <typename... Args>
        void operator()(auto &handler, std::exception_ptr error, Args &&...args) && {
            if (error) {
                try {
                    std::rethrow_exception(error);
                } catch (...) {
                    std::terminate();
                }
            }
            return std::move(handler)(std::forward<Args>(args)...);
        }
    };
};

} // namespace mcpp::asio::detail

namespace mcpp::asio {
template <typename CT>
inline auto transform_noexcept(CT &&token) {
    return detail::make_wrapped_token<detail::transform_noexcept_impl>(std::forward<CT>(token));
}
} // namespace mcpp::asio
