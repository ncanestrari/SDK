cmake_minimum_required(VERSION 3.15)
project(SDK)

set(CMAKE_CXX_STANDARD 20)
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

# Option to build shared or static library
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Create JsonParser library (can be STATIC or SHARED based on BUILD_SHARED_LIBS)
add_library(sdk
    src/json_node.cpp
    src/memory_manager.cpp
    src/object.cpp
    src/scheduler.cpp
    src/logger.cpp
    src/clock.cpp
    src/socket.cpp
    # Generated socket initialization files will be added after generation
)

target_include_directories(sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(sdk PUBLIC
    fmt::fmt
)

# Set version and soversion for shared library
set_target_properties(sdk PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
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

add_executable(clock_example
    example/clock_example.cpp
)

target_link_libraries(clock_example
    sdk
    fmt::fmt
)

add_executable(socket_example
    example/socket_example.cpp
)

target_link_libraries(socket_example
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

# Custom target to run code generator for examples
add_custom_command(
    OUTPUT generated/gameentity_initializer.hpp generated/gameentity_initializer.cpp generated/gameentity_.conf
           generated/configuration_initializer.hpp generated/configuration_initializer.cpp generated/configuration_.conf
           generated/playerstats_initializer.hpp generated/playerstats_initializer.cpp generated/playerstats_.conf
    COMMAND json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/example/json_init_example.hpp
            --output-dir generated
            -p ${CMAKE_CURRENT_BINARY_DIR}
            -- -std=c++20
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -resource-dir /usr/local/lib/clang/21
    DEPENDS json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/example/json_init_example.hpp
    COMMENT "Generating JSON initialization code for each class"
)

# Custom target to run code generator for socket classes
add_custom_command(
    OUTPUT generated/socket/unixstreamsocket_initializer.hpp generated/socket/unixstreamsocket_initializer.cpp generated/socket/unixstreamsocket_.conf
           generated/socket/unixstreamserversocket_initializer.hpp generated/socket/unixstreamserversocket_initializer.cpp generated/socket/unixstreamserversocket_.conf
           generated/socket/unixdatagramsocket_initializer.hpp generated/socket/unixdatagramsocket_initializer.cpp generated/socket/unixdatagramsocket_.conf
           generated/socket/tcpsocket_initializer.hpp generated/socket/tcpsocket_initializer.cpp generated/socket/tcpsocket_.conf
           generated/socket/tcpserversocket_initializer.hpp generated/socket/tcpserversocket_initializer.cpp generated/socket/tcpserversocket_.conf
           generated/socket/udpsocket_initializer.hpp generated/socket/udpsocket_initializer.cpp generated/socket/udpsocket_.conf
           generated/socket/rawsocket_initializer.hpp generated/socket/rawsocket_initializer.cpp generated/socket/rawsocket_.conf
           generated/socket/unixseqpacketsocket_initializer.hpp generated/socket/unixseqpacketsocket_initializer.cpp generated/socket/unixseqpacketsocket_.conf
           generated/socket/unixseqpacketserversocket_initializer.hpp generated/socket/unixseqpacketserversocket_initializer.cpp generated/socket/unixseqpacketserversocket_.conf
    COMMAND ${CMAKE_COMMAND} -E make_directory generated/socket
    COMMAND json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/include/socket.hpp
            --output-dir generated/socket
            -p ${CMAKE_CURRENT_BINARY_DIR}
            -- -std=c++20
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -resource-dir /usr/local/lib/clang/21
    DEPENDS json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/include/socket.hpp
    COMMENT "Generating JSON initialization code for socket classes"
)

# Custom target to run code generator for Scheduler class
add_custom_command(
    OUTPUT generated/scheduler/scheduler_initializer.hpp generated/scheduler/scheduler_initializer.cpp generated/scheduler/scheduler_.conf
    COMMAND ${CMAKE_COMMAND} -E make_directory generated/scheduler
    COMMAND json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/include/scheduler.hpp
            --output-dir generated/scheduler
            -p ${CMAKE_CURRENT_BINARY_DIR}
            -- -std=c++20
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -resource-dir /usr/local/lib/clang/21
    DEPENDS json_init_generator ${CMAKE_CURRENT_SOURCE_DIR}/include/scheduler.hpp
    COMMENT "Generating JSON initialization code for Scheduler class"
)

add_custom_target(generate_code
    DEPENDS generated/gameentity_initializer.hpp generated/gameentity_initializer.cpp generated/gameentity_.conf
            generated/configuration_initializer.hpp generated/configuration_initializer.cpp generated/configuration_.conf
            generated/playerstats_initializer.hpp generated/playerstats_initializer.cpp generated/playerstats_.conf
)

add_custom_target(generate_socket_code
    DEPENDS generated/socket/unixstreamsocket_initializer.cpp generated/socket/tcpsocket_initializer.cpp
            generated/socket/udpsocket_initializer.cpp generated/socket/rawsocket_initializer.cpp
            generated/socket/unixstreamserversocket_initializer.cpp generated/socket/tcpserversocket_initializer.cpp
            generated/socket/unixdatagramsocket_initializer.cpp generated/socket/unixseqpacketsocket_initializer.cpp
            generated/socket/unixseqpacketserversocket_initializer.cpp
)

add_custom_target(generate_scheduler_code
    DEPENDS generated/scheduler/scheduler_initializer.cpp
)

# Add generated socket source files to SDK library
target_sources(sdk PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/unixstreamsocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/unixstreamserversocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/unixdatagramsocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/tcpsocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/tcpserversocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/udpsocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/rawsocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/unixseqpacketsocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/unixseqpacketserversocket_initializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/scheduler/scheduler_initializer.cpp
)

# Make SDK library depend on socket and scheduler code generation
add_dependencies(sdk generate_socket_code generate_scheduler_code)

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
    target_compile_options(sdk PRIVATE -Wall -Wextra -Wpedantic -Wno-attributes -O0 -g)
    target_compile_options(json_example PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    target_compile_options(scheduler_example_graph PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    if(BUILD_CODE_GENERATOR)
        target_compile_options(json_init_generator PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
    endif()
endif()

# Installation
install(TARGETS sdk json_init_generator
    EXPORT SDKTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

# Install public headers
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json_node.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json_init_generator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/socket.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/memory_manager.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/object.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/scheduler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/logger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/clock.hpp
    DESTINATION include
)

# Install generated socket initialization files (headers and source)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/socket/
    DESTINATION include/generated/socket
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.cpp"
)

# Export targets for find_package
install(EXPORT SDKTargets
    FILE SDKTargets.cmake
    NAMESPACE SDK::
    DESTINATION lib/cmake/SDK
)

# Create and install package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SDKConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SDKConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SDKConfig.cmake"
    INSTALL_DESTINATION lib/cmake/SDK
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SDKConfigVersion.cmake"
    DESTINATION lib/cmake/SDK
)
