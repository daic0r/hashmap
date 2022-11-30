cmake_minimum_required(VERSION 3.24)

project(hashmap)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)

add_executable(hashmap main.cpp)

target_link_options(hashmap PRIVATE -fsanitize=address)