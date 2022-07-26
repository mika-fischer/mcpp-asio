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
#define MCPP_ASIO_ERROR_NAMESPACE boost::system
#else
#include <system_error>
#define MCPP_ASIO_NAMESPACE asio
#define MCPP_ASIO_CONT_HELPERS_NAMESPACE asio_handler_cont_helpers
#define MCPP_ASIO_ERROR_NAMESPACE std
#endif

namespace mcpp::asio {
using error_code = MCPP_ASIO_ERROR_NAMESPACE::error_code;
using system_error = MCPP_ASIO_ERROR_NAMESPACE::system_error;
} // namespace mcpp::asio