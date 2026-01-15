/**
 * @brief OpenGL显示管理类
 *
 * 该类负责OpenGL窗口管理、着色器编译、纹理处理和渲染操作。
 * 支持双纹理分屏显示，用于内窥镜的双目立体视觉渲染。
 */
#ifndef GLDISPLAY_H
#define GLDISPLAY_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

class GLDisplay {
public:
    GLDisplay();
    ~GLDisplay();

    /**
     * @brief 初始化OpenGL显示环境
     * @param width 窗口宽度
     * @param height 窗口高度
     * @param title 窗口标题
     * @return 初始化是否成功
     */
    bool init(int width, int height, std::string title);

    /**
     * @brief 设置双纹理（左右眼）
     * @param width 纹理宽度
     * @param height 纹理高度
     * @return 左纹理ID（用于兼容性）
     */
    unsigned int setupTexture(int width, int height);

    /**
     * @brief 更新双目视频纹理数据
     * @param leftData 左眼图像数据（BGR格式）
     * @param rightData 右眼图像数据（BGR格式）
     * @param width 图像宽度
     * @param height 图像高度
     */
    void updateVideo(unsigned char* leftData, unsigned char* rightData, int width, int height);

    /**
     * @brief 执行渲染操作
     */
    void draw();

    /**
     * @brief 检查窗口是否应该关闭
     * @return true如果窗口应该关闭
     */
    bool shouldClose();

    /**
     * @brief 清理OpenGL资源
     */
    void cleanup();

private:
    GLFWwindow* window;           // GLFW窗口句柄
    unsigned int shaderProgram;   // GLSL着色器程序
    unsigned int VAO, VBO;        // 顶点数组和缓冲对象
    unsigned int leftTexID, rightTexID;  // 左右眼纹理ID
    int windowWidth, windowHeight;       // 窗口尺寸

    /**
     * @brief 初始化GLFW窗口
     */
    bool initGLFW(int width, int height, std::string title);

    /**
     * @brief 初始化GLAD OpenGL函数加载器
     */
    bool initGLAD();

    /**
     * @brief 编译和链接GLSL着色器
     */
    bool compileShaders();

    /**
     * @brief 设置全屏四边形顶点数据
     */
    void setupQuad();

    // ========== GLSL着色器源码 ==========

    /**
     * 顶点着色器：处理2D位置和纹理坐标传递
     */
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;        // 顶点位置
        layout (location = 1) in vec2 aTexCoord;   // 纹理坐标

        out vec2 TexCoord;  // 传递给片段着色器的纹理坐标

        void main()
        {
            // 将2D坐标转换为标准化设备坐标(NDC)
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    /**
     * 片段着色器：实现双目分屏渲染
     *
     * 屏幕布局：
     * 左半屏 (UV.x < 0.5)：显示左眼纹理
     * 右半屏 (UV.x >= 0.5)：显示右眼纹理
     *
     * UV映射逻辑：
     * - 左半屏：UV.x 从 [0, 0.5] 映射到 [0, 1] (乘以2)
     * - 右半屏：UV.x 从 [0.5, 1.0] 映射到 [0, 1] (减0.5后乘以2)
     */
    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;  // 最终输出颜色

        in vec2 TexCoord;    // 从顶点着色器接收的纹理坐标

        uniform sampler2D texLeft;   // 左眼纹理采样器
        uniform sampler2D texRight;  // 右眼纹理采样器

        void main()
        {
            vec2 uv = TexCoord;

            if (uv.x < 0.5) {
                // 屏幕左半部分：采样左眼纹理
                // 将UV坐标从[0, 0.5]映射到[0, 1]
                vec2 leftUV = vec2(uv.x * 2.0, uv.y);
                FragColor = texture(texLeft, leftUV);
            } else {
                // 屏幕右半部分：采样右眼纹理
                // 将UV坐标从[0.5, 1.0]映射到[0, 1]
                vec2 rightUV = vec2((uv.x - 0.5) * 2.0, uv.y);
                FragColor = texture(texRight, rightUV);
            }
        }
    )";
};

#endif // GLDISPLAY_H
