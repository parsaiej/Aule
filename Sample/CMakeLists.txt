cmake_minimum_required(VERSION 3.28)

# Executable
# ----------------------

add_executable(AuleSample Main.cpp)

# Link
# -----------------------

target_include_directories(AuleSample PRIVATE ${CMAKE_SOURCE_DIR}/Include/)

# Link
# -----------------------

target_link_libraries(AuleSample PRIVATE Aule)