# Minimum cmake version required 
cmake_minimum_required(VERSION 3.8) 
 
# Help CMake to find the installed library on Windows or Mac OS X 
list(APPEND CMAKE_PREFIX_PATH "C:\\bullet") 
 
# Import the ReactPhysics3D library 
find_package(BULLET_PHYSICS REQUIRED)
 
# Project 
project(VulkanTest) 
 
# Create the executable 
add_executable(VulkanTest Main.cpp) 
 
# Link with the ReactPhysics3D library 
target_link_libraries(VulkanTest BULLET_PHYSICS::BULLET_PHYSICS)
