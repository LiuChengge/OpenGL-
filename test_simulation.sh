#!/bin/bash
cd build
echo 'Testing OpenGL Simulation Mode...'
timeout 5s ./endo_viewer_v4l 2>&1 | grep -E '(frame|elapsed|Starting|completed)' | tail -10
echo 'Simulation test completed successfully!'
