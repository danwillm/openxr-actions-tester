cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)

project(openxr_actions_tester)

add_subdirectory(lib/json)
add_subdirectory(lib/OpenXR-SDK)

add_executable(openxr_program src/main.cpp)

target_link_libraries(openxr_program PRIVATE openxr_loader nlohmann_json)