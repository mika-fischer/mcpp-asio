// Copyright Mika Fischer 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#if defined(__clang__) && !defined(ASIO_HAS_CO_AWAIT)
#define ASIO_HAS_CO_AWAIT 1
#endif

#define MCPP_ASIO_USE_BOOST 0
#include <mcpp/asio/config.hpp>
#include <mcpp/asio/transform_system_error.hpp>

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/experimental/promise.hpp>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <exception>
#include <system_error>

using namespace MCPP_ASIO_NAMESPACE;
using namespace mcpp::asio;

namespace {
auto noop() -> awaitable<void> {
    co_return;
}

auto sleep(std::chrono::milliseconds duration) -> awaitable<void> {
    auto timer = steady_timer(co_await this_coro::executor);
    timer.expires_from_now(duration);
    co_await timer.async_wait(use_awaitable);
}

// auto queue_wait() -> awaitable<void> {
//     auto queue = AsyncQueue<int>(0);
//     int val = 0;
//     co_await queue.async_pop(val, use_awaitable);
// }

auto throw_system_error() -> awaitable<int> {
    throw std::system_error(std::make_error_code(std::errc::timed_out));
    co_return 0;
}

auto throw_system_error_after(std::chrono::milliseconds duration) -> awaitable<int> {
    auto timer = steady_timer(co_await this_coro::executor);
    timer.expires_from_now(duration);
    co_await timer.async_wait(use_awaitable);
    throw std::system_error(std::make_error_code(std::errc::timed_out));
    co_return 0;
}

void test_cancelation(auto coro) {
    auto ioc = io_context();
    auto promise = co_spawn(ioc, std::move(coro), experimental::use_promise);
    auto timer = steady_timer(ioc);
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait([&](auto /*ec*/) { promise.cancel(); });
    promise.async_wait([&](std::exception_ptr ex) {
        REQUIRE(ex);
        try {
            std::rethrow_exception(ex);
        } catch (std::exception &e) {
            REQUIRE(typeid(e) == typeid(std::system_error));
            auto ec = dynamic_cast<std::system_error &>(e).code();
#ifdef _WIN32
            REQUIRE(ec == error_code(995, asio::system_category()));
#else
            REQUIRE(ec == std::errc::operation_canceled);
#endif
        } catch (...) {
            REQUIRE(false);
        }
    });
    ioc.run();
}

} // namespace

TEST_CASE("asio.co_spawn.completes_with_std_exception_ptr") {
    auto ioc = io_context();
    co_spawn(ioc, noop(), [](auto err) { REQUIRE(std::is_same_v<decltype(err), std::exception_ptr>); });
    ioc.run();
}

TEST_CASE("asio.co_spawn.can_be_canceled") {
    test_cancelation(sleep(std::chrono::seconds(1)));
}

// TEST_CASE("asio.co_spawn.custom_op_can_be_canceled") {
//     test_cancelation(queue_wait());
// }

TEST_CASE("asio.transform_system_error.direct") {
    auto ioc = io_context();
    co_spawn(ioc, throw_system_error(), transform_system_error([&](std::error_code ec, int i) {
                 REQUIRE(ec == std::errc::timed_out);
                 REQUIRE(i == 0);
             }));
    ioc.run();
}

TEST_CASE("asio.transform_system_error.promise") {
    auto ioc = io_context();
    auto promise = co_spawn(ioc, throw_system_error(), transform_system_error(experimental::use_promise));
    promise.async_wait([&](std::error_code ec, int i) {
        REQUIRE(ec == std::errc::timed_out);
        REQUIRE(i == 0);
    });
    ioc.run();
}

TEST_CASE("asio.transform_system_error.promise_can_be_canceled") {
    auto ioc = io_context();
    auto promise = co_spawn(ioc, throw_system_error_after(std::chrono::seconds(10)),
                            transform_system_error(experimental::use_promise));
    auto timer = steady_timer(ioc);
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait([&](auto /*ec*/) { promise.cancel(); });
    promise.async_wait([&](error_code ec, int i) {
        REQUIRE(ec != std::errc::timed_out);
#ifdef _WIN32
        REQUIRE(ec == error_code(995, asio::system_category()));
#else
        REQUIRE(ec == std::errc::operation_canceled);
#endif
    });
    ioc.run();
}

// TODO: Ensure that associated attributes are still correct...
// [x] Cancellation
// [ ] Executor