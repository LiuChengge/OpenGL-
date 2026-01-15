#include <GLFW/glfw3.h>
#include <iostream>
int main() {
    std::cout << "Testing GLFW..." << std::endl;
    if (!glfwInit()) {
        std::cout << "GLFW init failed" << std::endl;
        return 1;
    }
    std::cout << "GLFW version: " << glfwGetVersionString() << std::endl;
    glfwTerminate();
    std::cout << "GLFW test completed" << std::endl;
    return 0;
}
