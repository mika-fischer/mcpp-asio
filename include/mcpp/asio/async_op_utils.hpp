// Copyright Mika Fischer 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <mcpp/asio/config.hpp>

#if MCPP_ASIO_USE_BOOST
#include <boost/asio/associator.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/detail/handler_cont_helpers.hpp>
#else
#include <asio/associator.hpp>
#include <asio/async_result.hpp>
#include <asio/detail/handler_cont_helpers.hpp>
#endif

#include <type_traits>
#include <utility>

namespace mcpp::asio::detail {

template <typename T, typename U>
concept decays_to = std::same_as<std::decay_t<T>, U>;

template <typename T>
concept wrapped_handler_impl = requires(const T &impl) {
    std::is_object_v<T>;
};

template <typename Handler, wrapped_handler_impl Implementation>
struct wrapped_handler {
    Handler inner_handler_;
    [[no_unique_address]] Implementation implementation_;

    template <decays_to<Handler> H, typename TokenImpl>
        requires(std::is_default_constructible_v<Implementation> && !std::is_constructible_v<Implementation, TokenImpl>)
    explicit wrapped_handler(H &&handler, TokenImpl && /*token_impl*/)
        : inner_handler_(std::forward<H>(handler)), implementation_() {}

    template <decays_to<Handler> H, typename TokenImpl>
        requires(std::is_constructible_v<Implementation, TokenImpl>)
    explicit wrapped_handler(H &&handler, TokenImpl &&token_impl)
        : inner_handler_(std::forward<H>(handler)), implementation_(std::forward<TokenImpl>(token_impl)) {}

    template <typename... Args>
        requires(std::is_invocable_v<Implementation &&, Handler &, Args &&...>)
    auto operator()(Args &&...args) && {
        return std::move(implementation_)(inner_handler_, std::forward<Args>(args)...);
    }

    template <typename... Args>
        requires(!std::is_invocable_v<Implementation &&, Handler &, Args &&...>)
    auto operator()(Args &&...args) && { return std::move(inner_handler_)(std::forward<Args>(args)...); }
};

template <typename Handler, wrapped_handler_impl Implementation>
inline auto asio_handler_is_continuation(wrapped_handler<Handler, Implementation> *this_handler) -> bool {
    return MCPP_ASIO_CONT_HELPERS_NAMESPACE::is_continuation(this_handler->inner_handler());
}

template <typename T>
concept wrapped_token_impl = requires(const T &impl) {
    requires(wrapped_handler_impl<typename T::handler_impl>);
};

template <typename Signature, typename Impl>
struct transform_signature_impl {
    using type = Signature;
};

template <typename T, typename Signature>
concept can_transform_signature = requires(T) {
    typename T::template transform_signature<Signature>;
};

template <typename Signature, can_transform_signature<Signature> Impl>
struct transform_signature_impl<Signature, Impl> {
    using type = typename Impl::template transform_signature<Signature>;
};

template <typename Signature, typename Impl>
using transform_signature_t = typename transform_signature_impl<Signature, Impl>::type;

template <wrapped_token_impl Impl, typename CompletionToken>
struct wrapped_token {
    static_assert(!std::is_reference_v<CompletionToken>);

    [[no_unique_address]] CompletionToken inner_token_;
    [[no_unique_address]] Impl implementation_;

    template <typename Signature>
    using transform_signature = transform_signature_impl<Signature, Impl>;

    template <decays_to<CompletionToken> T, typename... Args>
    explicit wrapped_token(T &&token, Args &&...args)
        : inner_token_(std::forward<T>(token)), implementation_(std::forward<Args>(args)...) {}

    template <typename Executor>
    struct executor_with_default : Executor {
        using default_completion_token_type = wrapped_token;
        using Executor::Executor;
    };

    template <typename T>
    static auto as_default_on(T &&object) {
        using U = std::decay_t<T>;
        using new_executor = executor_with_default<typename U::executor_type>;
        return U::template rebind_executor<new_executor>::other(std::forward<T>(object));
    }
};

template <wrapped_token_impl Impl, typename CT, typename... Ts>
inline auto make_wrapped_token(CT &&token, Ts &&...args) {
    return wrapped_token<Impl, std::decay_t<CT>>(std::forward<CT>(token), std::forward<Ts>(args)...);
}

} // namespace mcpp::asio::detail

namespace MCPP_ASIO_NAMESPACE {

template <template <class, class> class Associator, class Handler, class Implementation, class Default>
struct associator<Associator, mcpp::asio::detail::wrapped_handler<Handler, Implementation>, Default>
    : Associator<Handler, Default> {
    using base = Associator<Handler, Default>;
    static auto get(const mcpp::asio::detail::wrapped_handler<Handler, Implementation> &handler,
                    const Default &d = {}) noexcept {
        return base::get(handler.inner_handler_, d);
    }
};

template <typename Impl, typename CT, typename... Ss>
struct async_result<mcpp::asio::detail::wrapped_token<Impl, CT>, Ss...>
    : async_result<CT, mcpp::asio::detail::transform_signature_t<Ss, Impl>...> {
    using base = async_result<CT, mcpp::asio::detail::transform_signature_t<Ss, Impl>...>;
    template <typename I, typename... Ts>
    static auto initiate(I &&init, auto &&token, Ts &&...args) {
        return base::initiate(
            [init = std::forward<I>(init),
             token_impl = std::move(token.implementation_)]<class H, class... Us>(H &&handler, Us &&...args2) mutable {
                using handler_impl = typename Impl::handler_impl;
                using wrapped_handler = mcpp::asio::detail::wrapped_handler<std::decay_t<H>, handler_impl>;
                return std::move(init)(wrapped_handler(std::forward<H>(handler), std::move(token_impl)),
                                       std::forward<Us>(args2)...);
            },
            std::move(token.inner_token_), std::forward<Ts>(args)...);
    }
};

} // namespace MCPP_ASIO_NAMESPACE

#define MCPP_ASIO_FOREACH_SIGNATURE_QUALIFIER(macro)                                                                   \
    macro(, ) macro(&, ) macro(&&, ) macro(, noexcept) macro(&, noexcept) macro(&&, noexcept)
