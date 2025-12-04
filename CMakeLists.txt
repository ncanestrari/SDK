cmake_minimum_required(VERSION 3.15)
project(SDK)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(fmt REQUIRED)
find_package(Clang REQUIRED)
find_package(LLVM REQUIRED CONFIG)

# Add LLVM/Clang definitions and include directories
llvm_map_components_to_libnames(llvm_libs support core irreader)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

# Create JsonParser library
add_library(sdk 
    src/json_node.cpp
    src/memory_manager.cpp
    src/object.cpp
    src/scheduler.cpp
)

target_include_directories(sdk PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sdk 
    fmt::fmt
)

# Create code generator tool
add_executable(json_init_generator
    src/json_init_generator.cpp
    src/json_init_generator_main.cpp
)

target_link_libraries(json_init_generator
    fmt::fmt
    clangTooling
    clangBasic
    clangAST
    clangASTMatchers
    clangFrontend
    clangRewrite
    clangSerialization
    ${llvm_libs}
)

# Create example executable
add_executable(json_example 
    example/json_node_example.cpp
)

target_link_libraries(json_example 
    sdk
    fmt::fmt
)

# Create test for generated code
add_executable(test_generated
    example/test_generated.cpp
    generated/gameentity_initializer.cpp
    generated/configuration_initializer.cpp  
    generated/playerstats_initializer.cpp
)

target_include_directories(test_generated PRIVATE generated/)

target_link_libraries(test_generated
    sdk
    fmt::fmt
)

# Custom target to run code generator
add_custom_command(
    OUTPUT generated/gameentity_initializer.hpp generated/gameentity_initializer.cpp generated/gameentity_.conf
           generated/configuration_initializer.hpp generated/configuration_initializer.cpp generated/configuration_.conf
           generated/playerstats_initializer.hpp generated/playerstats_initializer.cpp generated/playerstats_.conf
    COMMAND json_init_generator example/json_init_example.hpp --output-dir generated
    DEPENDS json_init_generator example/json_init_example.hpp
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
    target_compile_options(json_init_generator PRIVATE /W4)
else()
    target_compile_options(sdk PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(json_example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(json_init_generator PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
install(TARGETS sdk json_init_generator
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES json_node.hpp json_init_generator.hpp
    DESTINATION include
)
