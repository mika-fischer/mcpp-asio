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

template <typename T>
concept wrapped_handler_impl = requires(const T &impl) {
    typename T::inner_handler_type;
    { impl.inner_handler() } -> std::same_as<const typename T::inner_handler_type &>;
};

template <typename Handler>
requires(!std::is_reference_v<Handler>) struct wrapped_handler_impl_base {
    using inner_handler_type = Handler;
    auto inner_handler() const & -> const inner_handler_type & { return inner_handler_; }

  protected:
    explicit wrapped_handler_impl_base(inner_handler_type &&inner_handler) : inner_handler_(std::move(inner_handler)) {}
    explicit wrapped_handler_impl_base(const inner_handler_type &inner_handler) : inner_handler_(inner_handler) {}

    auto inner_handler() & -> inner_handler_type & { return inner_handler_; }
    auto inner_handler() && -> inner_handler_type && { return std::move(inner_handler_); }

    template <typename... Args>
    auto complete(Args &&...args) && {
        return std::move(inner_handler())(std::forward<Args>(args)...);
    }

  private:
    inner_handler_type inner_handler_;
};

template <wrapped_handler_impl Implementation>
struct wrapped_handler : Implementation {
    using inner_handler_type = typename Implementation::inner_handler_type;
    explicit wrapped_handler(inner_handler_type &&handler) : Implementation(std::move(handler)) {}
    explicit wrapped_handler(const inner_handler_type &handler) : Implementation(handler) {}
};

template <wrapped_handler_impl Implementation>
inline auto asio_handler_is_continuation(wrapped_handler<Implementation> *this_handler) -> bool {
    return MCPP_ASIO_CONT_HELPERS_NAMESPACE::is_continuation(this_handler->inner_handler());
}

template <typename Handler>
struct transparent_handler : wrapped_handler_impl_base<Handler> {
    using wrapped_handler_impl_base<Handler>::wrapped_handler_impl_base;

    template <typename... Args>
    auto operator()(Args &&...args) && {
        return std::move(*this).complete(std::forward<Args>(args)...);
    }
};

struct invocable_archetype {
    template <typename... Args>
    void operator()(Args &&...args) {}
};

template <typename T>
concept wrapped_token_impl = requires(const T &impl) {
    wrapped_handler_impl<typename T::template handler_impl<invocable_archetype>>;
    typename T::template transform_signature<void()>;
    // TODO: ICE on GCC 12. Need to find a better way to check this...
    // {[]<typename Signature>(typename T::template transform_signature<Signature> &) {}};
};

struct wrapped_token_impl_base {
    template <typename Handler>
    using handler_impl = transparent_handler<Handler>;
    template <typename Signature>
    using transform_signature = Signature;
};
static_assert(wrapped_token_impl<wrapped_token_impl_base>);

template <wrapped_token_impl Impl, typename CompletionToken>
requires(!std::is_reference_v<CompletionToken>) struct wrapped_token {
    CompletionToken inner_token_;

    explicit wrapped_token(CompletionToken &&token) : inner_token_(std::move(token)) {}
    explicit wrapped_token(const CompletionToken &token) : inner_token_(token) {}

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

} // namespace mcpp::asio::detail

namespace MCPP_ASIO_NAMESPACE {

template <template <class, class> class Associator, class Implementation, class Default>
struct associator<Associator, mcpp::asio::detail::wrapped_handler<Implementation>, Default>
    : Associator<typename mcpp::asio::detail::wrapped_handler<Implementation>::inner_handler_type, Default> {
    using base = Associator<typename mcpp::asio::detail::wrapped_handler<Implementation>::inner_handler_type, Default>;
    static auto get(const mcpp::asio::detail::wrapped_handler<Implementation> &handler,
                    const Default &d = {}) noexcept {
        return base::get(handler.inner_handler(), d);
    }
};

template <typename Impl, typename CT, typename... Ss>
struct async_result<mcpp::asio::detail::wrapped_token<Impl, CT>, Ss...>
    : async_result<CT, typename Impl::template transform_signature<Ss>...> {
    using base = async_result<CT, typename Impl::template transform_signature<Ss>...>;
    template <typename I, typename... Ts>
    static auto initiate(I &&init, auto &&token, Ts &&...args) {
        return base::initiate(
            [init = std::forward<I>(init)]<class H, class... Us>(H &&handler, Us &&...args2) mutable {
                using handler_impl = typename Impl::template handler_impl<std::decay_t<H>>;
                using wrapped_handler = mcpp::asio::detail::wrapped_handler<handler_impl>;
                return std::move(init)(wrapped_handler(std::forward<H>(handler)), std::forward<Us>(args2)...);
            },
            std::move(token.inner_token_), std::forward<Ts>(args)...);
    }
};

} // namespace MCPP_ASIO_NAMESPACE

#define MCPP_ASIO_FOREACH_SIGNATURE_QUALIFIER(macro)                                                                   \
    macro(, ) macro(&, ) macro(&&, ) macro(, noexcept) macro(&, noexcept) macro(&&, noexcept)
