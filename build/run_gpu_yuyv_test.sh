#!/bin/bash
echo "GPU加速YUYV转换测试"
echo "此版本将YUYV->RGB转换移至GPU着色器"
echo "预期性能提升：从~100ms降至~1-2ms"
echo ""
echo "按ESC关闭窗口"
echo ""

cd build
./endo_viewer_v4l
