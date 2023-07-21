#pragma once

#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <vulkan/vulkan.h>

/*
 * Various utility functions.
 * Memory management, math, vulkan config
 */

inline static std::vector<char> getBytes(const char* fileName) {
    std::ifstream fileStream(fileName, std::ios::ate | std::ios::binary);

    if (!fileStream.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    size_t fileSize = fileStream.tellg();
    std::cout << "File size: " << fileSize << "\n";

    std::vector<char> r(fileSize);

    fileStream.seekg(0);
    fileStream.read(r.data(), fileSize);
    fileStream.close();

    return r;
}