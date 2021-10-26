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

# not work :(
target_compile_options(
			${PROJECT_NAME}
			PRIVATE
			#`llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native`
			# vvv
			# -I/usr/lib/llvm-12/include -fno-exceptions -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS                                                                                                     
			#-L/usr/lib/llvm-12/lib                                                                                                  
			#-lLLVM-12 

			$<$<CXX_COMPILER_ID:CLANG>:-rdynamic>
			$<$<CXX_COMPILER_ID:GCC>:-Wl -export-dynamic>
)
