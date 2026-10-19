cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(defjam_hdpack LANGUAGES CXX)
endif()
add_executable(defjam_hdpack main.cpp)
set_target_properties(defjam_hdpack PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_features(defjam_hdpack PRIVATE cxx_std_20)
target_compile_definitions(defjam_hdpack PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(defjam_hdpack PRIVATE windowscodecs ole32)

# Optional runtime integration check, runnable without an ISO or the game's generated sources.
add_executable(defjam_hd_loader_test EXCLUDE_FROM_ALL
    test_loader.cpp ../../sdk/runtime/gpu/hd_textures.cpp)
target_compile_features(defjam_hd_loader_test PRIVATE cxx_std_20)
target_compile_definitions(defjam_hd_loader_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(defjam_hd_loader_test PRIVATE ../../sdk/runtime)
target_link_libraries(defjam_hd_loader_test PRIVATE windowscodecs ole32)
