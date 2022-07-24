// Copyright Mika Fischer 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#ifndef MCPP_ASIO_USE_BOOST
#define MCPP_ASIO_USE_BOOST 0
#endif

#if MCPP_ASIO_USE_BOOST
#include <boost/system/system_error.hpp>
#define MCPP_ASIO_NAMESPACE boost::asio
#define MCPP_ASIO_CONT_HELPERS_NAMESPACE boost_asio_handler_cont_helpers
#else
#include <system_error>
#define MCPP_ASIO_NAMESPACE asio
#define MCPP_ASIO_CONT_HELPERS_NAMESPACE asio_handler_cont_helpers
#endif

namespace mcpp::asio {
#if MCPP_ASIO_USE_BOOST
using error_code = boost::system::error_code;
using error_category = boost::system::error_category;
using system_error = boost::system::system_error;
inline auto system_category() noexcept -> const error_category & {
    return boost::system::system_category();
}
#else
using error_code = std::error_code;
using error_category = std::error_category;
using system_error = std::system_error;
inline auto system_category() noexcept -> const error_category & {
    return std::system_category();
}
#endif
} // namespace mcpp::asio