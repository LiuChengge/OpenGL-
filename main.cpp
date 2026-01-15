#include <cstdio>
#include <string>
#include "./src/endo_viewer.h"

int main(int argc, char* argv[])
{
    printf("================ Endoscope viewer startup (Simulation Mode with Real Image) ================\n");

    // Use original EndoViewer for OpenGL display mode
    EndoViewer endo_viewer;
    endo_viewer.startup(4, 6, false);
  //  endo_viewer.startup(6, 7, false);
    
    return 0;
}


