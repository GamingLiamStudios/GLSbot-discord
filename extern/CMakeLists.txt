include(FetchContent)

# zlib-ng
set(ZLIB_ENABLE_TESTS OFF)
set(WITH_NATIVE_INSTRUCTIONS ON)

# cpr
set(BUILD_CPR_TESTS OFF)
set(BUILD_CPR_TESTS_SSL OFF)
set(USE_SYSTEM_CURL ON)

# base64
set(BASE64_INSTALL_TARGET OFF)
set(BASE64_BUILD_TESTS OFF)

FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng
        GIT_TAG        2.0.2
)

# TODO: Remove CPR, replace with just plain curl
FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/whoshuu/cpr
        GIT_TAG        1.5.2
)

FetchContent_Declare(
        base64
        GIT_REPOSITORY https://github.com/madebr/base64
        GIT_TAG        574b9d1bfce170c99aa418ee45ce4f4fce953305
)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG        7.0.3
)

FetchContent_MakeAvailable(fmt base64 cpr zlib)
