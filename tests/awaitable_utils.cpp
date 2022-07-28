#include <stdexcept>
#if defined(__clang__) && !defined(ASIO_HAS_CO_AWAIT)
#define ASIO_HAS_CO_AWAIT 1
#endif

#include <mcpp/asio/awaitable_utils.hpp>

#include <asio/detached.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <exception>
#include <iostream>
#include <system_error>

using namespace ::MCPP_ASIO_NAMESPACE;
using namespace mcpp::asio;
using namespace std::literals;
using std::chrono::milliseconds;

namespace {
auto sleep_for(std::chrono::milliseconds duration) -> awaitable<int> {
    auto timer = steady_timer(co_await this_coro::executor);
    timer.expires_from_now(duration);
    co_await timer.async_wait(use_awaitable);
    co_return 0;
}

auto throw_after(std::chrono::milliseconds duration) -> awaitable<void> {
    auto timer = steady_timer(co_await this_coro::executor);
    timer.expires_from_now(duration);
    co_await timer.async_wait(use_awaitable);
    std::cerr << "THROWING" << std::endl;
    throw std::runtime_error("throw_after");
}
} // namespace

TEST_CASE("race.success") {
    auto ioc = io_context();
    co_spawn(
        ioc,
        [&]() -> awaitable<void> {
            try {
                auto result = co_await race(sleep_for(1ms), sleep_for(10ms));
                REQUIRE(result.index() == 0);
                REQUIRE(std::get<0>(result) == 0);
            } catch (...) {
                REQUIRE(false);
            }
        },
        detached);
    ioc.run();
}

TEST_CASE("race.failure") {
    auto ioc = io_context();
    co_spawn(
        ioc,
        [&]() -> awaitable<void> {
            try {
                co_await race(throw_after(1ms), sleep_for(10ms));
                REQUIRE(false);
            } catch (std::runtime_error &e) {
                REQUIRE(e.what() == "throw_after"sv);
            } catch (...) {
                REQUIRE(false);
            }
        },
        detached);
    ioc.run();
}

TEST_CASE("all.success") {
    auto ioc = io_context();
    co_spawn(
        ioc,
        [&]() -> awaitable<void> {
            try {
                auto result = co_await all(sleep_for(1ms), sleep_for(5ms));
            } catch (...) {
                REQUIRE(false);
            }
        },
        detached);
    ioc.run();
}

TEST_CASE("all.failure") {
    auto ioc = io_context();
    co_spawn(
        ioc,
        [&]() -> awaitable<void> {
            try {
                co_await all(throw_after(1ms), sleep_for(10ms));
                REQUIRE(false);
            } catch (std::runtime_error &e) {
                REQUIRE(e.what() == "throw_after"sv);
            } catch (...) {
                REQUIRE(false);
            }
        },
        detached);
    ioc.run();
}

TEST_CASE("all_settled.success") {
    auto ioc = io_context();
    co_spawn(
        ioc,
        [&]() -> awaitable<void> {
            try {
                auto result = co_await all_settled(sleep_for(1ms), sleep_for(5ms));
            } catch (...) {
                REQUIRE(false);
            }
        },
        detached);
    ioc.run();
}

TEST_CASE("all_settled.failure") {
    auto ioc = io_context();
    co_spawn(
        ioc,
        [&]() -> awaitable<void> {
            try {
                auto result = co_await all_settled(throw_after(1ms), sleep_for(10ms));
                REQUIRE(std::get<0>(result).index() == 1);
                REQUIRE(std::get<1>(result).index() == 0);
                REQUIRE(std::get<0>(std::get<1>(result)) == 0);
            } catch (std::exception &e) {
                REQUIRE(e.what() == ""sv);
            } catch (...) {
                REQUIRE(false);
            }
        },
        detached);
    ioc.run();
}
