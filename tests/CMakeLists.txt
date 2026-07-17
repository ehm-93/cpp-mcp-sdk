# Tests use only the standard library so the SDK keeps a small dependency surface.
add_executable(mcp-sdk-tests sdk_tests.cpp)
target_link_libraries(mcp-sdk-tests PRIVATE mcp::sdk)
add_test(NAME mcp-sdk-tests COMMAND mcp-sdk-tests)
