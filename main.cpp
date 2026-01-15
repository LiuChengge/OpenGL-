#include <cstdio>
#include <string>
#include "./src/endo_viewer.h"

int main(int argc, char* argv[])
{
    printf("================ Endoscope viewer startup (Simulation Mode with Real Image) ================\n");

    // Enable real camera mode for latency testing (use false for real cameras)
    EndoViewer endo_viewer(false);
    endo_viewer.startup(4, 6, false);
  //  endo_viewer.startup(6, 7, false);
    
    return 0;
}


