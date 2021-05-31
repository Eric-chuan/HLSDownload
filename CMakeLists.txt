cmake_minimum_required(VERSION 3.5)

project(hlsdl LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wstrict-prototypes -Wmissing-prototypes
    -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual")


add_definitions(-DCMAKE)

INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/include")
include_directories(/usr/include)

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")



add_executable(hlsdl ${SOURCES})

TARGET_LINK_LIBRARIES(hlsdl
        dl
        -lcurl)