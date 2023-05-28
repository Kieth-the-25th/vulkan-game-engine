# Minimum cmake version required 
cmake_minimum_required(VERSION 3.8) 
 
list(APPEND CMAKE_PREFIX_PATH "C:\\bullet") 

find_package(BULLET_PHYSICS REQUIRED)

project(VulkanTest) 
 
add_executable(VulkanTest Main.cpp) 

target_link_libraries(VulkanTest BULLET_PHYSICS::BULLET_PHYSICS)
