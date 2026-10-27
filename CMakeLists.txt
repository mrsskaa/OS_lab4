cmake_minimum_required(VERSION 3.16)
project(OS_lab4 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(parent src/parent.c)
add_executable(child src/child.c)
add_dependencies(parent child)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parent PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(child PRIVATE -Wall -Wextra -Wpedantic)
endif()
