cmake_minimum_required(VERSION 3.16)
PROJECT(MiniServer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 自动搜src下所有cpp
file(GLOB SRC_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp")

add_executable(http_server ${SRC_FILES})