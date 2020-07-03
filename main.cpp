// May 2, 2020

#include <iostream>
#include <exception>

#include "VulkanApp.hpp"

int main() {
    VulkanApp app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}