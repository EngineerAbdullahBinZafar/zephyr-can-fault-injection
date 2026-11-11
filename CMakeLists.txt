cmake_minimum_required(VERSION 3.16)
project(zephyr_can_rate_limiter LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(zephyr_can_rate_limiter STATIC
    src/zephyr_can_rate_limiter.c
)

target_include_directories(zephyr_can_rate_limiter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(zephyr_can_rate_limiter PRIVATE
    -Wall -Wextra -Werror -Wpedantic
)

enable_testing()
add_executable(test_zephyr_can_c tests/test_zephyr_can_c.c)
target_link_libraries(test_zephyr_can_c PRIVATE zephyr_can_rate_limiter)
target_compile_options(test_zephyr_can_c PRIVATE -Wall -Wextra -Werror -Wpedantic)
add_test(NAME zephyr_can_c_tests COMMAND test_zephyr_can_c)
