# A real executable doubles as usage documentation and a stdio smoke-test target.
add_executable(mcp-calculator-server calculator_server.cpp)
target_link_libraries(mcp-calculator-server PRIVATE mcp::sdk)
