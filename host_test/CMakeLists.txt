# Host (PC) build of the hardware-free components and their unit tests.
# This is separate from the ESP-IDF build; see README.md in this folder.
cmake_minimum_required(VERSION 3.16)
project(doorlock_host_tests C)

set(CMAKE_C_STANDARD 99)
set(COMP ${CMAKE_CURRENT_SOURCE_DIR}/../components)

add_executable(host_tests
    test_main.c
    ${COMP}/access_ctrl/access_ctrl.c
    ${COMP}/face_db/face_db.c
    ${COMP}/access_log/access_log.c
)
target_include_directories(host_tests PRIVATE
    ${COMP}/access_ctrl/include
    ${COMP}/face_db/include
    ${COMP}/access_log/include
)
if(NOT MSVC)
    target_compile_options(host_tests PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(host_tests PRIVATE m)
endif()

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
