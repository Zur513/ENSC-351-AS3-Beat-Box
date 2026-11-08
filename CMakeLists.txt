# CMake Build Configuration for root of project
cmake_minimum_required(VERSION 3.18)
project(assignment_2 VERSION 1.0 DESCRIPTION "ENSC351 Assignment 2" LANGUAGES C)

# Compiler options (inherited by sub-folders)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Werror -Wpedantic -Wextra)
add_compile_options(-fdiagnostics-color)

# AddressSanitizer for development
# Off by default
option(USE_ASAN "Build with AddressSanitizer" OFF)
if(USE_ASAN)
	add_compile_options(-fsanitize=address)
	add_link_options(-fsanitize=address)
endif()

# Enable PThread library for linking
add_compile_options(-pthread)
add_link_options(-pthread)

# Which folders to build
add_subdirectory(hal)
add_subdirectory(app)

