#!/bin/bash
echo "启动内窥镜查看器 - OpenGL双目模拟模式"
echo "窗口大小: 800x600 (分屏显示)"
echo "您将看到:"
echo "  - 左侧: 红色方块从左向右移动"
echo "  - 右侧: 蓝色方块从右向左移动"
echo "按 ESC 或关闭窗口退出程序"
echo ""

cd build
./endo_viewer_v4l
