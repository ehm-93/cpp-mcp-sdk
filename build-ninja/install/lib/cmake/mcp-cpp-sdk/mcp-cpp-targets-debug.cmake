#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mcp::sdk" for configuration "Debug"
set_property(TARGET mcp::sdk APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(mcp::sdk PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/mcp-cpp.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS mcp::sdk )
list(APPEND _IMPORT_CHECK_FILES_FOR_mcp::sdk "${_IMPORT_PREFIX}/lib/mcp-cpp.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
