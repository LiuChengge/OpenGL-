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
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class GLDisplay {
public:
    GLDisplay();
    ~GLDisplay();

    /**
     * @brief 初始化OpenGL显示环境
     * @param width 窗口宽度
     * @param height 窗口高度
     * @param title 窗口标题
     * @param numWindows 窗口数量（默认为1）
     * @return 初始化是否成功
     */
    bool init(int width, int height, std::string title, int numWindows = 1);

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
     * @brief 执行渲染操作（单窗口，保持向后兼容）
     */
    void draw();

    /**
     * @brief 串行渲染所有窗口（单线程顺序渲染）
     */
    void drawSerial();

    /**
     * @brief 并行渲染所有窗口（多线程同时渲染）
     */
    void drawParallel();

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
    std::vector<GLFWwindow*> windows;    // GLFW窗口句柄向量
    std::vector<unsigned int> VAOs;      // 每个窗口的VAO（VAO在OpenGL 3.3 Core Profile中不共享）
    unsigned int shaderProgram;         // GLSL着色器程序（共享）
    unsigned int VBO, EBO;              // 顶点缓冲对象和索引缓冲对象（共享）
    unsigned int leftTexID, rightTexID;  // 左右眼纹理ID（共享）
    int windowWidth, windowHeight;       // 窗口尺寸

    // 着色器uniform位置缓存（避免多线程中重复查询）
    int texLeftLocation;                 // texLeft uniform位置
    int texRightLocation;                // texRight uniform位置

    // 线程池相关成员变量
    std::vector<std::thread> workers;           // 持久线程池
    std::mutex mtx;                             // 同步互斥锁
    std::condition_variable cv_start;           // 启动工作条件变量
    std::condition_variable cv_done;            // 工作完成条件变量
    std::atomic<int> threads_completed{0};      // 已完成线程计数
    bool stop_threads = false;                  // 线程停止标志
    uint64_t frame_gen_id = 0;                  // 帧代计数器（确保每帧只处理一次）

    /**
     * @brief 初始化GLFW窗口
     * @param width 窗口宽度
     * @param height 窗口高度
     * @param title 窗口标题
     * @param numWindows 窗口数量
     */
    bool initGLFW(int width, int height, std::string title, int numWindows);

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
     * @param windowIndex 窗口索引（用于存储对应的VAO）
     */
    void setupQuad(int windowIndex);

    /**
     * @brief 在指定窗口上下文中执行渲染（用于多线程渲染）
     * @param windowIndex 窗口索引
     */
    void renderWindowContext(int windowIndex);

    /**
     * @brief 初始化持久线程池
     */
    void initWorkers();

    /**
     * @brief 工作线程循环
     * @param windowIndex 窗口索引
     */
    void workerLoop(int windowIndex);

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
