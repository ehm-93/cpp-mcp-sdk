cmake_minimum_required(VERSION 3.20)

project(mcp-cpp-sdk VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(MCP_CPP_BUILD_EXAMPLES "Build mcp-cpp-sdk examples" ON)
option(MCP_CPP_BUILD_TESTS "Build mcp-cpp-sdk tests" ON)
option(MCP_CPP_FETCH_DEPENDENCIES "Fetch missing dependencies" ON)

# Prefer a package supplied by the consumer; FetchContent is a convenience for
# standalone builds and can be disabled for fully offline/package-manager use.
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
  if(NOT MCP_CPP_FETCH_DEPENDENCIES)
    message(FATAL_ERROR "nlohmann_json 3.11 or newer is required")
  endif()
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(mcp-cpp
  src/client.cpp
  src/server.cpp
  src/stdio_transport.cpp
)
add_library(mcp::sdk ALIAS mcp-cpp)

# C++20 is part of the public contract because callbacks expose std::stop_token.
target_compile_features(mcp-cpp PUBLIC cxx_std_20)
# nlohmann_json is header-only. Copying its build include path avoids exporting
# a fetched dependency target; the installed config reconnects the real target.
get_target_property(MCP_CPP_JSON_INCLUDE_DIRS
  nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(mcp-cpp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${MCP_CPP_JSON_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(mcp-cpp PROPERTIES
  EXPORT_NAME sdk
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(MSVC)
  target_compile_options(mcp-cpp PRIVATE /W4 /permissive-)
else()
  target_compile_options(mcp-cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MCP_CPP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(MCP_CPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(TARGETS mcp-cpp EXPORT mcp-cpp-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT mcp-cpp-targets
  FILE mcp-cpp-targets.cmake
  NAMESPACE mcp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp-cpp-sdk
)
configure_package_config_file(
  cmake/mcp-cpp-sdk-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/mcp-cpp-sdk-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp-cpp-sdk
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/mcp-cpp-sdk-config-version.cmake
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/mcp-cpp-sdk-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/mcp-cpp-sdk-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp-cpp-sdk
)
