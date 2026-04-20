cmake_minimum_required(VERSION 3.15)
project(ZonVM VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)
file(GLOB_RECURSIVE SOURCES "src/*.cpp")

add_executable(zonvm main.cpp ${SOURCES})
if(MSVC)
    target_compile_options(zonvm PRIVATE /W4 /O2 /permissive-)
else()
    target_compile_options(zonvm PRIVATE 
        -Wall -Wextra -Wpedantic
        -O3
        -march=native 
        -fno-exceptions
        -fno-rtti
    )
endif()