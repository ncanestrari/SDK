cmake_minimum_required(VERSION 3.15)
project(SDK)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/")

include(FetchContent)

# Find or fetch fmt library
find_package(fmt QUIET)
find_package(Clang)
find_package(LLVM CONFIG)
#if(NOT fmt_FOUND)
#    message(STATUS "fmt not found, fetching from GitHub...")
#    FetchContent_Declare(
#        fmt
#        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
#        GIT_TAG 10.2.1
#    )
#    FetchContent_MakeAvailable(fmt)
#endif()

# Option to fetch LLVM/Clang from source (disabled by default due to build time)
#option(FETCH_LLVM "Fetch and build LLVM/Clang from source (WARNING: Very slow!)" OFF)
#
## Try to find Clang/LLVM (optional for code generator)
#set(CLANG_LIBS_FOUND FALSE)
#
#if(NOT FETCH_LLVM)
#    find_package(Clang)
#    find_package(LLVM CONFIG)
#
#    # Check if Clang libraries actually exist
#    if(Clang_FOUND AND LLVM_FOUND)
#        # Try to verify that the actual libraries can be linked
#        include(CheckCXXSourceCompiles)
#        set(CMAKE_REQUIRED_LIBRARIES clangTooling clangBasic)
#        check_cxx_source_compiles("
#            #include <clang/Tooling/Tooling.h>
#            int main() { return 0; }
#        " CLANG_LIBS_AVAILABLE)
#
#        if(CLANG_LIBS_AVAILABLE)
#            set(CLANG_LIBS_FOUND TRUE)
#            set(BUILD_CODE_GENERATOR ON)
#            # Add LLVM/Clang definitions and include directories
#            llvm_map_components_to_libnames(llvm_libs support core irreader)
#            add_definitions(${LLVM_DEFINITIONS})
#            include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
#            message(STATUS "Clang/LLVM libraries verified - json_init_generator will be built")
#        else()
#            message(STATUS "Clang/LLVM found but libraries are missing")
#        endif()
#    endif()
#endif()

# If system libraries not found or FETCH_LLVM is ON, fetch from source
#if(NOT CLANG_LIBS_FOUND AND FETCH_LLVM)
#    message(STATUS "Fetching LLVM/Clang from GitHub...")
#    message(WARNING "Building LLVM/Clang from source takes 1-2 hours and requires 20+ GB disk space!")
#
#    set(LLVM_ENABLE_PROJECTS "clang" CACHE STRING "")
#    set(LLVM_TARGETS_TO_BUILD "host" CACHE STRING "")
#    set(LLVM_BUILD_TOOLS OFF CACHE BOOL "")
#    set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "")
#    set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
#    set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
#    set(LLVM_ENABLE_RTTI ON CACHE BOOL "")
#    set(LLVM_ENABLE_EH ON CACHE BOOL "")
#
#    FetchContent_Declare(
#        llvm
#        GIT_REPOSITORY https://github.com/llvm/llvm-project.git
#        GIT_TAG llvmorg-17.0.6
#        GIT_SHALLOW TRUE
#        SOURCE_SUBDIR llvm
#    )
#
#    FetchContent_MakeAvailable(llvm)
#
#    set(BUILD_CODE_GENERATOR ON)
#    set(CLANG_LIBS_FOUND TRUE)
#
#    # Set up include directories
#    include_directories(
#        ${llvm_SOURCE_DIR}/llvm/include
#        ${llvm_BINARY_DIR}/llvm/include
#        ${llvm_SOURCE_DIR}/clang/include
#        ${llvm_BINARY_DIR}/clang/include
#    )
#
#    # Link to clang libraries
#    set(llvm_libs LLVMSupport LLVMCore LLVMIRReader)
#
#    message(STATUS "LLVM/Clang fetched and built - json_init_generator will be built")
#endif()

#if(NOT CLANG_LIBS_FOUND)
#    set(BUILD_CODE_GENERATOR OFF)
#    message(STATUS "Clang/LLVM libraries not available - json_init_generator will not be built")
#    message(STATUS "To enable code generator, use: cmake -DFETCH_LLVM=ON ..")
#    message(STATUS "WARNING: Building LLVM/Clang from source is very slow (1-2 hours)")
#endif()

# Create JsonParser library
add_library(sdk
    src/json_node.cpp
    src/memory_manager.cpp
    src/object.cpp
    src/scheduler.cpp
    src/logger.cpp
)

target_include_directories(sdk PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sdk 
    fmt::fmt
)

# Create code generator tool (only if Clang/LLVM available)
#if(BUILD_CODE_GENERATOR)
add_executable(json_init_generator
    src/json_init_generator.cpp
    src/json_init_generator_main.cpp
)

# Add LLVM definitions (includes -fno-rtti removal)
separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND "${LLVM_DEFINITIONS}")
target_compile_definitions(json_init_generator PRIVATE ${LLVM_DEFINITIONS_LIST})

# Enable RTTI and exceptions for Clang/LLVM compatibility
target_compile_options(json_init_generator PRIVATE -fno-rtti -fexceptions)

target_include_directories(json_init_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# Map LLVM components to library names
llvm_map_components_to_libnames(llvm_libs Support Core)

target_link_libraries(json_init_generator
    clangTooling
    clangBasic
    clangAST
    clangASTMatchers
    clangFrontend
    clangRewrite
    clangSerialization
    ${llvm_libs}
    fmt::fmt
)
#endif()

# Create example executables
add_executable(json_example
    example/json_node_example.cpp
)

target_link_libraries(json_example
    sdk
    fmt::fmt
)

add_executable(scheduler_example_graph
    example/scheduler_example_graph.cpp
)

target_link_libraries(scheduler_example_graph
    sdk
    fmt::fmt
)

add_executable(scheduler_example
    example/scheduler_example.cpp
)

target_link_libraries(scheduler_example
    sdk
    fmt::fmt
)

add_executable(logger_example
    example/logger_example.cpp
)

target_link_libraries(logger_example
    sdk
    fmt::fmt
)

# Create test for generated code (only if code generator is available)
#if(BUILD_CODE_GENERATOR)
add_executable(test_generated
    example/test_generated.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/gameentity_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/configuration_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/playerstats_initializer.cpp
)

target_include_directories(test_generated PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/
    ${CMAKE_CURRENT_SOURCE_DIR}/example
)

target_link_libraries(test_generated
    sdk
    fmt::fmt
)

# Custom target to run code generator
add_custom_command(
    OUTPUT generated/gameentity_initializer.hpp generated/gameentity_initializer.cpp generated/gameentity_.conf
           generated/configuration_initializer.hpp generated/configuration_initializer.cpp generated/configuration_.conf
           generated/playerstats_initializer.hpp generated/playerstats_initializer.cpp generated/playerstats_.conf
    COMMAND json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/example/json_init_example.hpp
            --output-dir generated
            -p ${CMAKE_CURRENT_BINARY_DIR}
            -- -std=c++17
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -resource-dir /usr/local/lib/clang/21
    DEPENDS json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/example/json_init_example.hpp
    COMMENT "Generating JSON initialization code for each class"
)

add_custom_target(generate_code
    DEPENDS generated/gameentity_initializer.hpp generated/gameentity_initializer.cpp generated/gameentity_.conf
            generated/configuration_initializer.hpp generated/configuration_initializer.cpp generated/configuration_.conf
            generated/playerstats_initializer.hpp generated/playerstats_initializer.cpp generated/playerstats_.conf
)

# Make test_generated depend on code generation
add_dependencies(test_generated generate_code)

# Compiler-specific options
if(MSVC)
    target_compile_options(sdk PRIVATE /W4)
    target_compile_options(json_example PRIVATE /W4)
    target_compile_options(scheduler_example_graph PRIVATE /W4)
    if(BUILD_CODE_GENERATOR)
        target_compile_options(json_init_generator PRIVATE /W4)
    endif()
else()
    target_compile_options(sdk PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    target_compile_options(json_example PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    target_compile_options(scheduler_example_graph PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    if(BUILD_CODE_GENERATOR)
        target_compile_options(json_init_generator PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    endif()
endif()

# Installation
#if(BUILD_CODE_GENERATOR)
install(TARGETS sdk json_init_generator
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
#else()
#    install(TARGETS sdk
#        RUNTIME DESTINATION bin
#        LIBRARY DESTINATION lib
#        ARCHIVE DESTINATION lib
#    )
#endif()

install(FILES json_node.hpp json_init_generator.hpp
    DESTINATION include
)
