cmake_minimum_required(VERSION 2.8)

# Set the plugin name to build
project(der_scheduler)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

message(STATUS ${CMAKE_CXX_FLAGS})

# Generation version header file
set_source_files_properties(version.h PROPERTIES GENERATED TRUE)
add_custom_command(
  OUTPUT version.h
  DEPENDS ${CMAKE_SOURCE_DIR}/VERSION
  COMMAND ${CMAKE_SOURCE_DIR}/mkversion ${CMAKE_SOURCE_DIR}
  COMMENT "Generating version header"
  VERBATIM
)
include_directories(${CMAKE_BINARY_DIR})

# Find source files
#file(GLOB SOURCES *.cpp)
file(GLOB SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/scheduler/*.c)

message(STATUS ${SOURCES})

# Add ./include
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/scheduler)

# Create shared library
add_library(der_scheduler STATIC ${SOURCES} version.h)

set_target_properties(der_scheduler PROPERTIES LINKER_LANGUAGE C)

target_link_libraries(der_scheduler -L/usr/local/lib -liec61850)

# Add additional libraries
target_link_libraries(der_scheduler -lpthread -ldl)

# Set the build version 
set_target_properties(der_scheduler PROPERTIES SOVERSION 1)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/examples)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests)
