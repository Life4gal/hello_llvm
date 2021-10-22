project(
		hello_kaleidoscope
		LANGUAGES CXX
)

# We incorporate the CMake features provided by LLVM:
if($ENV{LLVM_ROOT})
	set(LLVM_ROOT $ENV{LLVM_ROOT})
endif()
find_package(LLVM PATHS "${LLVM_ROOT}/lib/cmake/llvm" "$ENV{ProgramFiles}/lib/cmake/llvm")

if(LLVM_FOUND)
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
else()
	message(FATAL_ERROR "LLVM installation not found. Set LLVM_ROOT to the installation root path.")
endif(LLVM_FOUND)

include(LLVMConfig)
message(STATUS "Found LLVM Package Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM Built Type: ${LLVM_BUILD_TYPE}")

llvm_map_components_to_libnames(
	REQ_LLVM_LIBRARIES 
	Analysis
	Core
	ExecutionEngine
	InstCombine
	Object
	OrcJIT
	RuntimeDyld
	ScalarOpts
	Support
	native
)

set(
		${PROJECT_NAME}_SOURCE
		src/lexer.cpp
		src/ast.cpp
		src/parser.cpp
)

add_library(
		${PROJECT_NAME}
		STATIC
		${${PROJECT_NAME}_SOURCE}
)

target_include_directories(
		${PROJECT_NAME}
		PUBLIC
		${PROJECT_SOURCE_DIR}/include
)

target_include_directories(
		${PROJECT_NAME}
		PUBLIC
		${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
		${LLVM_DEFINITIONS}
)

message("LLVM -----> ${LLVM_INCLUDE_DIRS}")

#target_include_directories(
#		${PROJECT_NAME}
#		PRIVATE
#		${CMAKE_CURRENT_BINARY_DIR}
#)

target_compile_features(
	${PROJECT_NAME} 
	PRIVATE 
	cxx_std_20
)

target_link_libraries(
	${PROJECT_NAME} 
	PRIVATE
	${REQ_LLVM_LIBRARIES}
)

include(${HELLO_LLVM_MODULE_PATH}/config_build_type.cmake)
