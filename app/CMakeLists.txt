project(
		kaleidoscope_app
		LANGUAGES CXX
)

add_executable(
		${PROJECT_NAME}
		src/main.cpp
)

target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
		hello_kaleidoscope
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

include(${HELLO_LLVM_MODULE_PATH}/config_build_type.cmake)
