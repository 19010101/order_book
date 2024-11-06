cmake_minimum_required(VERSION 3.13)
project(fin) 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SOURCE_FILES src/main.C)
add_compile_options(-Wall -Wextra -Wpedantic -ggdb)

### find_package(Boost 1.83 REQUIRED COMPONENTS filesystem)
find_package(Boost 1.83 REQUIRED)
find_package(Catch2 REQUIRED)

add_executable(temp ${SOURCE_FILES})
target_include_directories(temp PRIVATE ${Boost_INCLUDE_DIRS})
### target_link_libraries(temp Boost::filesystem)

add_executable(tests src/test.C)
target_link_libraries(tests Catch2::Catch2)


### include(CTest)
### include(ParseAndAddCatchTests)
### ParseAndAddCatchTests(tests)


