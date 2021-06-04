cmake_minimum_required(VERSION 3.9)
project(GLSbot-discord)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
option(BUILD_SHARED_LIBS "" off)

# Find OpenSSL
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

add_subdirectory(extern)

set(SOURCE_FILES
    # MAIN
    source/main.cpp

    # WEBSOCKET
    source/websocket/socket.cpp
    source/websocket/ws.cpp

    # GLSBOT
    source/GLSbot.cpp

    # DISCORDAPI
    source/discord-api/gateway.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC "extern")
target_include_directories(${PROJECT_NAME} PUBLIC "source") # to allow paths relative to project root

IF (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32)
ENDIF ()
target_link_libraries(${PROJECT_NAME} PRIVATE zlib OpenSSL::SSL OpenSSL::Crypto cpr::cpr base64)
