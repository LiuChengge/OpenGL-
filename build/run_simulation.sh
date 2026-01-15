#!/bin/bash
echo "启动内窥镜查看器 - OpenGL模拟模式"
echo "窗口大小: 800x600"
echo "您将看到一个绿色的100x100方块在黑色背景上从左向右水平移动"
echo "按 ESC 或关闭窗口退出程序"
echo ""

cd build
./endo_viewer_v4l
