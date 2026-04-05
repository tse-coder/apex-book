cmake_minimum_required(VERSION 3.14)
project(ApexBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Google Benchmark
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(benchmark)

# Crow for WebSocket
FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.0+5
)
FetchContent_MakeAvailable(Crow)

include_directories(include)

# Benchmarks
add_executable(benchmark_engine tests/benchmark_test.cpp)
target_link_libraries(benchmark_engine PRIVATE benchmark::benchmark benchmark::benchmark_main pthread)

# Main Engine Server
add_executable(apex_server src/main.cpp)
target_link_libraries(apex_server PRIVATE Crow::Crow pthread)
