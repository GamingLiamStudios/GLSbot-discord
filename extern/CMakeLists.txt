include(FetchContent)

# Include zlib-ng
FetchContent_Declare(
        zlib_ng
        GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng
        GIT_TAG        v2.0.0-RC1
)
FetchContent_GetProperties(zlib_ng)
if(NOT zlib_ng_POPULATED)
  FetchContent_Populate(zlib_ng)

  # Set custom variables, policies, etc.
  set(ZLIB_ENABLE_TESTS OFF)
  set(WITH_NATIVE_INSTRUCTIONS ON)

  add_subdirectory(${zlib_ng_SOURCE_DIR} ${zlib_ng_BINARY_DIR})
endif()

FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/whoshuu/cpr
        GIT_TAG        1.5.2
)
FetchContent_GetProperties(cpr)
if(NOT cpr_POPULATED)
  FetchContent_Populate(cpr)

  # Disable tests
  set(BUILD_CPR_TESTS OFF)
  set(BUILD_CPR_TESTS_SSL OFF)

  add_subdirectory(${cpr_SOURCE_DIR} ${cpr_BINARY_DIR})
endif()
