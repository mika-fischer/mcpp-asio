include(FetchContent)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/onqtam/doctest
    GIT_TAG        v2.4.8
)
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio
    GIT_TAG        asio-1-23-0 
)
FetchContent_MakeAvailable(doctest asio)
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

add_executable(test-asio asio.cpp)
target_link_libraries(test-asio PRIVATE mcpp::asio doctest_with_main)
target_include_directories(test-asio PUBLIC ${asio_SOURCE_DIR}/asio/include)
doctest_discover_tests(test-asio)