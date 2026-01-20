#include <cstdio>
#include <string>
#include "./src/endo_viewer.h"
#include "./src/inc/VkDisplay.h"

void testVulkan() {
    printf("================ Vulkan Backend Test ================\n");
    printf("Vulkan backend test skipped - implementation in progress.\n");
}

int main(int argc, char* argv[])
{
    printf("================ Endoscope viewer startup (Simulation Mode with Real Image) ================\n");

    // Test Vulkan backend first
    testVulkan();

    // Then run original EndoViewer for OpenGL display mode
    EndoViewer endo_viewer;
    endo_viewer.startup(0, 2, false);
    // endo_viewer.startup(6, 7, false);
    
    return 0;
}


