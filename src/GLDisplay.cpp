#include "inc/GLDisplay.h"
#include <iostream>
#include <cstdio>
#include <stb_image.h>
#include "efficiency_test.h"
#include <cstdlib>

GLDisplay::GLDisplay() : shaderProgram(0), VBO(0), EBO(0), windowWidth(0), windowHeight(0),
    texLeftLocation(-1), texRightLocation(-1) {
    // 初始化帧追踪数组
    for (int i = 0; i < MAX_TRACKED_FRAMES; i++) {
        frame_fences[i] = nullptr;
    }
}

GLDisplay::~GLDisplay() {
    cleanup();
}

bool GLDisplay::init(int width, int height, std::string title, int numWindows) {
    windowWidth = width;
    windowHeight = height;

    // 初始化GLFW窗口系统
    if (!initGLFW(width, height, title, numWindows)) {
        return false;
    }

    printf("VSync Enabled (Interval 1)\n");

    // 使用第一个窗口的上下文初始化GLAD和编译着色器（资源是共享的）
    glfwMakeContextCurrent(windows[0]);
    if (!initGLAD()) {
        return false;
    }

    // 编译和链接GLSL着色器程序（在第一个上下文中，资源会被共享）
    if (!compileShaders()) {
        return false;
    }

    // 为每个窗口设置VAO（VAO在OpenGL 3.3 Core Profile中不共享，需要为每个上下文创建）
    VAOs.resize(numWindows);
    for (int i = 0; i < numWindows; i++) {
        glfwMakeContextCurrent(windows[i]);
        setupQuad(i);
    }

    // 初始化每窗口的 fence / swap 状态（initGLFW 已经创建 windows 列表）
    window_frame_fences.resize(windows.size());
    window_swap_timestamps.resize(windows.size());
    for (size_t i = 0; i < windows.size(); ++i) {
        window_frame_fences[i] = nullptr;
    }

    // 设置背景清除颜色为黑色（在第一个窗口上下文中）
    glfwMakeContextCurrent(windows[0]);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // 释放主线程上下文，允许 worker 线程绑定并长期持有各自窗口上下文
    glfwMakeContextCurrent(NULL);

    return true;
}

bool GLDisplay::initGLFW(int width, int height, std::string title, int numWindows) {
    // 初始化GLFW库
    if (!glfwInit()) {
        return false;
    }

    // 配置OpenGL上下文版本和核心模式
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);  // 使窗口可见（用于性能测试）

    windows.resize(numWindows);
    
    // 创建第一个窗口（不使用共享上下文）
    windows[0] = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    if (!windows[0]) {
        glfwTerminate();
        return false;
    }

    // 设置第一个窗口的上下文并启用垂直同步（用于VSync阻塞测试）
    glfwMakeContextCurrent(windows[0]);
    glfwSwapInterval(1);

    // 创建后续窗口，使用第一个窗口作为共享上下文
    for (int i = 1; i < numWindows; i++) {
        std::string windowTitle = title + " - Window " + std::to_string(i + 1);
        windows[i] = glfwCreateWindow(width, height, windowTitle.c_str(), NULL, windows[0]);
        if (!windows[i]) {
            // 清理已创建的窗口
            for (int j = 0; j < i; j++) {
                glfwDestroyWindow(windows[j]);
            }
            glfwTerminate();
            return false;
        }

        // 设置窗口位置，避免重叠
        int xpos, ypos;
        glfwGetWindowPos(windows[0], &xpos, &ypos);
        glfwSetWindowPos(windows[i], xpos + width * i, ypos);

        // 为每个窗口设置上下文并启用垂直同步
        glfwMakeContextCurrent(windows[i]);
        glfwSwapInterval(1);
    }
    
    return true;
}

bool GLDisplay::initGLAD() {
    // 初始化GLAD OpenGL函数加载器
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return false;
    }
    return true;
}

bool GLDisplay::compileShaders() {
    // 编译顶点着色器
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // 检查顶点着色器编译状态
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }

    // 编译片段着色器
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // 检查片段着色器编译状态
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }

    // 创建着色器程序并链接
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // 检查程序链接状态
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }

    // 清理着色器对象（已链接到程序中）
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 缓存uniform位置（在单线程初始化时获取，避免多线程并发查询）
    texLeftLocation = glGetUniformLocation(shaderProgram, "texLeft");
    texRightLocation = glGetUniformLocation(shaderProgram, "texRight");

    return true;
}

void GLDisplay::setupQuad(int windowIndex) {
    // 全屏四边形顶点数据（标准化设备坐标NDC: -1到1）
    float vertices[] = {
        // 位置坐标        // 纹理坐标
         1.0f,  1.0f,   1.0f, 1.0f,   // 右上角
         1.0f, -1.0f,   1.0f, 0.0f,   // 右下角
        -1.0f, -1.0f,   0.0f, 0.0f,   // 左下角
        -1.0f,  1.0f,   0.0f, 1.0f    // 左上角
    };

    // 索引数据（两个三角形组成四边形）
    unsigned int indices[] = {
        0, 1, 3,   // 第一个三角形
        1, 2, 3    // 第二个三角形
    };

    // 为当前窗口生成VAO（VAO不共享，需要为每个上下文创建）
    glGenVertexArrays(1, &VAOs[windowIndex]);
    
    // 只在第一个窗口时创建VBO和EBO（它们是共享的）
    if (windowIndex == 0) {
    glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
    glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }

    // 绑定当前窗口的VAO
    glBindVertexArray(VAOs[windowIndex]);

    // 绑定共享的VBO（所有窗口共享同一个VBO）
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // 绑定共享的EBO（所有窗口共享同一个EBO，但每个VAO需要绑定它）
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标属性 (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑缓冲区（但保持VAO绑定，因为EBO绑定存储在VAO中）
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // 注意：不要解绑EBO，因为它绑定在VAO中
    glBindVertexArray(0);
}

unsigned int GLDisplay::setupTexture(int width, int height) {
    // 临时绑定第一个窗口的上下文，确保在没有长期绑定上下文时仍能创建纹理资源
    if (!windows.empty()) {
        glfwMakeContextCurrent(windows[0]);
    }

    // 创建左眼纹理
    glGenTextures(1, &leftTexID);
    glBindTexture(GL_TEXTURE_2D, leftTexID);
    // 分配纹理内存并初始化为白色，以便验证渲染管线（避免黑屏由空纹理引起）
    {
        size_t sz = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
        std::vector<unsigned char> white(sz, 255);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, white.data());
    }
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 创建右眼纹理
    glGenTextures(1, &rightTexID);
    glBindTexture(GL_TEXTURE_2D, rightTexID);
    {
        size_t sz = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
        std::vector<unsigned char> white(sz, 255);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, white.data());
    }
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 释放临时绑定的上下文（恢复到无上下文，保持 worker 的持久绑定不被干扰）
    if (!windows.empty()) {
        glfwMakeContextCurrent(NULL);
    }

    // 所有 GL 资源已创建，安全地启动 worker 线程（worker 将长期持有各自上下文）
    initWorkers();

    return leftTexID; // 返回左纹理ID以保持兼容性
}

void GLDisplay::updateVideo(unsigned char* leftData, unsigned char* rightData, int width, int height) {
    // 不在主线程进行任何 GL 调用，改为仅更新指针/尺寸供 worker 线程在其持久上下文中上传
    std::lock_guard<std::mutex> lock(mtx);
    currentLeftData = leftData;
    currentRightData = rightData;
    currentImgWidth = width;
    currentImgHeight = height;
}

void GLDisplay::draw() {
    // 向后兼容：只绘制第一个窗口
    if (windows.empty() || VAOs.empty()) return;
    
    glfwMakeContextCurrent(windows[0]);
    
    // 清除颜色缓冲区
    glClear(GL_COLOR_BUFFER_BIT);

    // 使用着色器程序
    glUseProgram(shaderProgram);
    // 绑定顶点数组对象
    glBindVertexArray(VAOs[0]);

    // 绑定左眼纹理到纹理单元0
    // 在持久上下文中上传最新的纹理数据（如果有）— 先从共享状态读取指针
    const unsigned char* leftPtr = nullptr;
    const unsigned char* rightPtr = nullptr;
    int imgW = 0, imgH = 0;
    {
        std::lock_guard<std::mutex> lock(mtx);
        leftPtr = currentLeftData;
        rightPtr = currentRightData;
        imgW = currentImgWidth;
        imgH = currentImgHeight;
    }

    static auto last_upload_log = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    if (leftPtr && imgW > 0 && imgH > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, leftTexID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgW, imgH, GL_RGB, GL_UNSIGNED_BYTE, leftPtr);

        // 每秒打印一次上传心跳，帮助确认上传确实发生
        auto now = std::chrono::steady_clock::now();
        if (now - last_upload_log >= std::chrono::seconds(1)) {
            last_upload_log = now;
            printf("Uploading texture frame...\n");
        }

        // 检查并打印任何 GL 错误（上传后）
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            const char* errstr = "UNKNOWN";
            switch (err) {
                case GL_INVALID_ENUM: errstr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errstr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errstr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errstr = "GL_OUT_OF_MEMORY"; break;
                default: break;
            }
            fprintf(stderr, "GL error after glTexSubImage2D: 0x%X (%s)\n", err, errstr);
        }
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, leftTexID);
    }
    glUniform1i(texLeftLocation, 0);

    // 绑定并可能上传右眼纹理
    if (rightPtr && imgW > 0 && imgH > 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rightTexID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgW, imgH, GL_RGB, GL_UNSIGNED_BYTE, rightPtr);

        // 检查并打印任何 GL 错误（上传后）
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            const char* errstr = "UNKNOWN";
            switch (err) {
                case GL_INVALID_ENUM: errstr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errstr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errstr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errstr = "GL_OUT_OF_MEMORY"; break;
                default: break;
            }
            fprintf(stderr, "GL error after glTexSubImage2D (right): 0x%X (%s)\n", err, errstr);
        }
    } else {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rightTexID);
    }
    glUniform1i(texRightLocation, 1);

    // 绘制全屏四边形（6个顶点）
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    // 检查并打印任何 GL 错误（绘制后）
    {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            const char* errstr = "UNKNOWN";
            switch (err) {
                case GL_INVALID_ENUM: errstr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errstr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errstr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errstr = "GL_OUT_OF_MEMORY"; break;
                default: break;
            }
            fprintf(stderr, "GL error after glDrawElements: 0x%X (%s)\n", err, errstr);
        }
    }

    //glFlush();
    glFinish();

    // 交换前后缓冲区并处理事件
    glfwSwapBuffers(windows[0]);
    glfwPollEvents();
}

void GLDisplay::drawSerial() {
    // 串行渲染：遍历所有窗口并顺序渲染
    for (size_t i = 0; i < windows.size() && i < VAOs.size(); i++) {
        glfwMakeContextCurrent(windows[i]);
        
        // 清除颜色缓冲区
        glClear(GL_COLOR_BUFFER_BIT);

        // 使用着色器程序
        glUseProgram(shaderProgram);
        // 绑定当前窗口的VAO
        glBindVertexArray(VAOs[i]);

        // 绑定左眼纹理到纹理单元0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, leftTexID);
        glUniform1i(texLeftLocation, 0);

        // 绑定右眼纹理到纹理单元1
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rightTexID);
        glUniform1i(texRightLocation, 1);

        // 绘制全屏四边形（6个顶点）
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // 确保命令执行完成
        glFinish();

        // 交换前后缓冲区
        glfwSwapBuffers(windows[i]);
    }
    
    // 处理所有窗口的事件（只需要调用一次）
    glfwPollEvents();
}

void GLDisplay::renderWindowContext(int windowIndex) {
    // 在指定窗口的上下文中执行渲染（用于多线程）
    if (windowIndex < 0 || windowIndex >= static_cast<int>(windows.size()) ||
        windowIndex >= static_cast<int>(VAOs.size())) {
        return;
    }

    // ===== 测试1：帧间隔测量 =====
    // 只在第一个窗口中测量帧间隔，避免多线程干扰
    static auto last_frame_time = std::chrono::steady_clock::now();
    if (windowIndex == 0) {
        auto current_time = std::chrono::steady_clock::now();
        auto frame_interval = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - last_frame_time).count();
        last_frame_time = current_time;
#if DO_EFFECIENCY_TEST
        printf("FRAME_INTERVAL: %ld us (%.2f ms)\n", frame_interval, frame_interval / 1000.0);
#endif
    }

    // 获取当前窗口的上下文
    glfwMakeContextCurrent(windows[windowIndex]);

    // 运行时诊断：打印当前上下文/平台/驱动信息，帮助定位 VSync/swap-interval 行为问题
    // 这些日志仅在 DO_EFFECIENCY_TEST 打开时输出（避免默认干扰）
#if DO_EFFECIENCY_TEST
    {
        // 当前 GLFW 上下文指针
        void* current_ctx = reinterpret_cast<void*>(glfwGetCurrentContext());
        void* win_ptr = reinterpret_cast<void*>(windows[windowIndex]);
        EFF_PRINT("CONTEXT_DIAG: windowIndex=%d current_ctx=%p window_ptr=%p\n",
                  windowIndex, current_ctx, win_ptr);

        // 环境与会话类型（Wayland vs X11）
        const char* xdg_session = getenv("XDG_SESSION_TYPE");
        const char* wayland = getenv("WAYLAND_DISPLAY");
        const char* display_env = getenv("DISPLAY");
        const char* vblank_mode = getenv("vblank_mode");
        const char* gl_sync = getenv("__GL_SYNC_TO_VBLANK");
        EFF_PRINT("ENV: XDG_SESSION_TYPE=%s WAYLAND_DISPLAY=%s DISPLAY=%s vblank_mode=%s __GL_SYNC_TO_VBLANK=%s\n",
                  xdg_session ? xdg_session : "(null)",
                  wayland ? wayland : "(null)",
                  display_env ? display_env : "(null)",
                  vblank_mode ? vblank_mode : "(null)",
                  gl_sync ? gl_sync : "(null)");

        // OpenGL 驱动/渲染器信息（在有当前上下文时有效）
        const unsigned char* vendor = glGetString(GL_VENDOR);
        const unsigned char* renderer = glGetString(GL_RENDERER);
        const unsigned char* version = glGetString(GL_VERSION);
        const unsigned char* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
        EFF_PRINT("GL_INFO: VENDOR=\"%s\" RENDERER=\"%s\" VERSION=\"%s\" GLSL=\"%s\"\n",
                  vendor ? vendor : (const unsigned char*)"(null)",
                  renderer ? renderer : (const unsigned char*)"(null)",
                  version ? version : (const unsigned char*)"(null)",
                  glsl ? glsl : (const unsigned char*)"(null)");

        // 检查常见 swap-control 扩展（可帮助判断是否支持 runtime swap interval 控制）
        EFF_PRINT("EXT_CHECK: WGL_EXT_swap_control=%d GLX_EXT_swap_control=%d GLX_MESA_swap_control=%d GLX_SGI_swap_control=%d\n",
                  glfwExtensionSupported("WGL_EXT_swap_control"),
                  glfwExtensionSupported("GLX_EXT_swap_control"),
                  glfwExtensionSupported("GLX_MESA_swap_control"),
                  glfwExtensionSupported("GLX_SGI_swap_control"));
    }
#endif

    // 立即确保在当前线程/上下文上启用 VSync
    glfwSwapInterval(1);

    // 清除颜色缓冲区（默认黑色背景）
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 使用着色器程序
    glUseProgram(shaderProgram);
    // 绑定当前窗口的VAO
    glBindVertexArray(VAOs[windowIndex]);

    // 在持久上下文中上传最新的纹理数据（如果有）— 先从共享状态读取指针
    const unsigned char* leftPtr = nullptr;
    const unsigned char* rightPtr = nullptr;
    int imgW = 0, imgH = 0;
    {
        std::lock_guard<std::mutex> lock(mtx);
        leftPtr = currentLeftData;
        rightPtr = currentRightData;
        imgW = currentImgWidth;
        imgH = currentImgHeight;
    }

    static auto last_upload_log = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    if (leftPtr && imgW > 0 && imgH > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, leftTexID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgW, imgH, GL_RGB, GL_UNSIGNED_BYTE, leftPtr);

        // 每秒打印一次上传心跳，帮助确认上传确实发生
        auto now = std::chrono::steady_clock::now();
        if (now - last_upload_log >= std::chrono::seconds(1)) {
            last_upload_log = now;
            printf("Uploading texture frame...\n");
        }

        // 检查并打印任何 GL 错误（上传后）
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            const char* errstr = "UNKNOWN";
            switch (err) {
                case GL_INVALID_ENUM: errstr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errstr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errstr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errstr = "GL_OUT_OF_MEMORY"; break;
                default: break;
            }
            fprintf(stderr, "GL error after glTexSubImage2D: 0x%X (%s)\n", err, errstr);
        }
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, leftTexID);
    }
    glUniform1i(texLeftLocation, 0);

    // 绑定并可能上传右眼纹理
    if (rightPtr && imgW > 0 && imgH > 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rightTexID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgW, imgH, GL_RGB, GL_UNSIGNED_BYTE, rightPtr);

        // 检查并打印任何 GL 错误（上传后）
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            const char* errstr = "UNKNOWN";
            switch (err) {
                case GL_INVALID_ENUM: errstr = "GL_INVALID_ENUM"; break;
                case GL_INVALID_VALUE: errstr = "GL_INVALID_VALUE"; break;
                case GL_INVALID_OPERATION: errstr = "GL_INVALID_OPERATION"; break;
                case GL_OUT_OF_MEMORY: errstr = "GL_OUT_OF_MEMORY"; break;
                default: break;
            }
            fprintf(stderr, "GL error after glTexSubImage2D (right): 0x%X (%s)\n", err, errstr);
        }
    } else {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rightTexID);
    }
    glUniform1i(texRightLocation, 1);

    // 绘制全屏四边形（6个顶点）
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // 确保命令执行完成（关键：用于延迟测试）
    glFinish();

    // 在工作线程的当前上下文上执行 SwapBuffers（在工作线程执行 swap 可提高驱动对 swap-interval 的一致性）
    // 确保在当前上下文上启用 VSync（部分驱动要求在 swap 的同一线程/上下文上设置）
    glfwSwapInterval(1);

    // 记录 swap 时间戳
    {
        std::lock_guard<std::mutex> lock(mtx);
        window_swap_timestamps[windowIndex] = std::chrono::steady_clock::now();
    }

    // 执行交换（在工作线程）
    glfwSwapBuffers(windows[windowIndex]);

    // 在当前上下文插入 fence，用于后续延迟检测
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (window_frame_fences[windowIndex] != nullptr) {
            glDeleteSync(window_frame_fences[windowIndex]);
            window_frame_fences[windowIndex] = nullptr;
        }
        window_frame_fences[windowIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    // 释放当前上下文
    glfwMakeContextCurrent(NULL);
}

void GLDisplay::initWorkers() {
    // 初始化持久线程池，为每个窗口创建一个工作线程
    if (windows.empty()) {
        return;
    }

    workers.resize(windows.size());
    for (size_t i = 0; i < windows.size(); i++) {
        workers[i] = std::thread(&GLDisplay::workerLoop, this, static_cast<int>(i));
    }
}

void GLDisplay::workerLoop(int windowIndex) {
    uint64_t local_gen_id = 0;
    // 绑定并长期持有该线程的窗口上下文，避免每帧重复绑定/解绑导致驱动打断 VSync 锁
    if (windowIndex < 0 || windowIndex >= static_cast<int>(windows.size())) {
        return;
    }
    glfwMakeContextCurrent(windows[windowIndex]);
    // 在此线程绑定的上下文上启用 VSync（swap interval）
    glfwSwapInterval(1);

    while (true) {
        std::unique_lock<std::mutex> lock(mtx);

        // 等待新的帧代或停止信号
        cv_start.wait(lock, [this, &local_gen_id]() {
            return frame_gen_id > local_gen_id || stop_threads;
        });

        if (stop_threads) {
            break;  // 退出循环，线程结束
        }

        // 更新本地代计数器以匹配全局代计数器
        local_gen_id = frame_gen_id;

        // 解锁以允许其他线程处理并并行渲染
        lock.unlock();

        // 执行渲染工作（renderWindowContext 已处理上下文管理）
        renderWindowContext(windowIndex);

        // 重新锁定以更新完成状态
        lock.lock();
        threads_completed++;

        // 当所有线程都完成后，通知主线程
        if (threads_completed == static_cast<int>(windows.size())) {
            cv_done.notify_one();
        }
    }
    // 退出循环前释放绑定的上下文
    glfwMakeContextCurrent(NULL);
}

void GLDisplay::drawParallel() {
    // 并行渲染：使用持久线程池同时渲染所有窗口
    if (windows.empty() || VAOs.empty()) {
        return;
    }

    // 主线程释放任何持有的上下文
    glfwMakeContextCurrent(NULL);

    // 递增帧代计数器并重置完成计数
    {
        std::unique_lock<std::mutex> lock(mtx);
        threads_completed = 0;
        frame_gen_id++;  // 新的帧代，通知所有工作线程
    }

    // 通知所有工作线程开始渲染
    cv_start.notify_all();

    // 等待所有工作线程完成渲染
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv_done.wait(lock, [this]() {
            return threads_completed == static_cast<int>(windows.size());
        });
    }

    // 主线程只负责处理窗口事件（SwapBuffers 已由工作线程在各自上下文中执行）
    glfwPollEvents();
}

void GLDisplay::checkFrameLatency() {
    // 检查每个窗口最近一帧的延迟（使用每窗口 fence）
    for (size_t i = 0; i < windows.size(); ++i) {
        GLsync fence = nullptr;
        std::chrono::steady_clock::time_point swap_time;
        {
            std::lock_guard<std::mutex> lock(mtx);
            fence = window_frame_fences[i];
            swap_time = window_swap_timestamps[i];
        }

        if (fence == nullptr) continue;

        GLenum fence_status = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (fence_status == GL_ALREADY_SIGNALED || fence_status == GL_CONDITION_SATISFIED) {
            auto completion_time = std::chrono::steady_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                completion_time - swap_time).count();

            #if DO_EFFECIENCY_TEST
            printf("FRAME_LATENCY: Window %zu - SwapBuffers to GPU completion: %ld us (%.2f ms)\n",
                   i, latency_us, latency_us / 1000.0);
            #endif

            // 清理完成的 fence
            {
                std::lock_guard<std::mutex> lock(mtx);
                glDeleteSync(window_frame_fences[i]);
                window_frame_fences[i] = nullptr;
            }
        } else if (fence_status == GL_TIMEOUT_EXPIRED) {
            // 还未完成，跳过
            continue;
        } else if (fence_status == GL_WAIT_FAILED) {
            #if DO_EFFECIENCY_TEST
            printf("FRAME_LATENCY: Error checking fence for window %zu\n", i);
            #endif
            {
                std::lock_guard<std::mutex> lock(mtx);
                glDeleteSync(window_frame_fences[i]);
                window_frame_fences[i] = nullptr;
            }
        }
    }
}

bool GLDisplay::shouldClose() {
    // 如果任何一个窗口应该关闭，返回true
    for (auto* window : windows) {
        if (glfwWindowShouldClose(window)) {
            return true;
        }
    }
    return false;
}

void GLDisplay::cleanup() {
    // 停止所有工作线程
    {
        std::unique_lock<std::mutex> lock(mtx);
        stop_threads = true;
        frame_gen_id++;  // 递增代计数器，确保等待中的线程检查停止条件
    }
    cv_start.notify_all();

    // 等待所有工作线程结束
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();

    // 清理所有GLsync fence对象
    for (int i = 0; i < MAX_TRACKED_FRAMES; i++) {
        if (frame_fences[i] != nullptr) {
            glDeleteSync(frame_fences[i]);
            frame_fences[i] = nullptr;
        }
    }

    // 清理所有VAO（需要在各自的上下文中删除）
    for (size_t i = 0; i < windows.size() && i < VAOs.size(); i++) {
        if (VAOs[i] != 0) {
            glfwMakeContextCurrent(windows[i]);
            glDeleteVertexArrays(1, &VAOs[i]);
        }
    }
    VAOs.clear();

    // 清理VBO和EBO（在第一个上下文中删除即可，因为共享）
    if (VBO != 0 && !windows.empty()) {
        glfwMakeContextCurrent(windows[0]);
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO != 0 && !windows.empty()) {
        glfwMakeContextCurrent(windows[0]);
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }

    // 清理着色器程序（在第一个上下文中删除即可，因为共享）
    if (shaderProgram != 0 && !windows.empty()) {
        glfwMakeContextCurrent(windows[0]);
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }

    // 清理纹理（在第一个上下文中删除即可，因为共享）
    if (leftTexID != 0 && !windows.empty()) {
        glfwMakeContextCurrent(windows[0]);
        glDeleteTextures(1, &leftTexID);
        leftTexID = 0;
    }
    if (rightTexID != 0 && !windows.empty()) {
        glfwMakeContextCurrent(windows[0]);
        glDeleteTextures(1, &rightTexID);
        rightTexID = 0;
    }

    // 销毁所有窗口
    for (auto* window : windows) {
    if (window) {
        glfwDestroyWindow(window);
        }
    }
    windows.clear();

    // 终止GLFW
    glfwTerminate();
}
