#message("in gtest")
project(gtest_builder C CXX)
include(ExternalProject)

ExternalProject_Add(gtest
    URL https://github.com/google/googletest/archive/release-1.7.0.zip
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
    INSTALL_COMMAND ""
)

# Specify include dir
ExternalProject_Get_Property(gtest source_dir)
set(GTEST_INCLUDE_DIRS ${source_dir}/include PARENT_SCOPE)

# Specify MainTest's link libraries
ExternalProject_Get_Property(gtest binary_dir)
set(GTEST_LIBS_DIR ${binary_dir} PARENT_SCOPE)
