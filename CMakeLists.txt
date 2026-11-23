# CMake Build Configuration for root of project
cmake_minimum_required(VERSION 3.18)

# Always cross-compile for the BeagleY-AI, even if no toolchain file is given
# on the command line (must come before project())
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/aarch64-toolchain.cmake")
endif()

project(beatbox VERSION 1.0 DESCRIPTION "ENSC351 Assignment 3" LANGUAGES C)

# Compiler options (inherited by sub-folders)
set(CMAKE_C_STANDARD 11)
# add_compile_options(-Wall -Werror -Wpedantic -Wextra)
# add_compile_options(-fdiagnostics-color)

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

