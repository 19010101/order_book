cmake_minimum_required(VERSION 3.13)
project(fin) 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# list(APPEND CMAKE_INCLUDE_PATH "/opt/local/libexec/boost/1.81/lib/cmake/Boost-1.81.0")
if (APPLE) 
    # set(OpenMP_ROOT "/opt/local/")
    set(Boost_DIR "/opt/local/libexec/boost/1.81/lib/cmake/Boost-1.81.0")
    set(fmt_DIR "/opt/local/lib/libfmt10/cmake/fmt/")
elseif (WIN32)
endif() 
set(SOURCE_FILES src/main.C)
add_compile_options(-Wall -Wextra -Wpedantic -ggdb)

### find_package(Boost 1.83 REQUIRED COMPONENTS filesystem)
# find_package(OpenMP REQUIRED)
find_package(Boost REQUIRED)
find_package(Catch2 3 REQUIRED)
find_package(spdlog REQUIRED)

add_executable(main ${SOURCE_FILES})
target_link_libraries(main PUBLIC Boost::boost spdlog::spdlog_header_only)
#target_include_directories(main PRIVATE ${Boost_INCLUDE_DIRS} spd)

add_executable(tests src/test.C)
#target_include_directories(tests PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(tests PUBLIC Catch2::Catch2WithMain Boost::boost spdlog::spdlog_header_only)

add_executable(tests_rng src/tests_rng.C)
target_link_libraries(tests_rng PUBLIC Catch2::Catch2WithMain Boost::boost spdlog::spdlog_header_only)

### include(CTest)
### include(ParseAndAddCatchTests)
### ParseAndAddCatchTests(tests)


