cmake_minimum_required(VERSION 3.15)
project(ethbot CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_executable(ethbot
    src/main.cpp
    src/config.cpp
    src/data_client.cpp
    src/ema.cpp
    src/strategy.cpp
    src/http.cpp
    src/rsi.cpp
    src/bollinger.cpp
    src/metrics.cpp
    src/order_placer.cpp
    src/risk.cpp
    src/logger.cpp
)

target_include_directories(ethbot PRIVATE include)
target_link_libraries(ethbot PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
