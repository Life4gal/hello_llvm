cmake_minimum_required(VERSION 3.13)

project(
		hello_llvm
		LANGUAGES CXX
)

set(HELLO_LLVM_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_subdirectory(kaleidoscope)
add_subdirectory(app)
