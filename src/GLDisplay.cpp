#include "inc/GLDisplay.h"
#include <iostream>
#include <cstdio>
#include <stb_image.h>

GLDisplay::GLDisplay() : shaderProgram(0), VBO(0), EBO(0), windowWidth(0), windowHeight(0),
    texLeftLocation(-1), texRightLocation(-1) {
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

    // 初始化持久线程池（用于并行渲染）
    initWorkers();

    // 设置背景清除颜色为黑色（在第一个窗口上下文中）
    glfwMakeContextCurrent(windows[0]);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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
    // 创建左眼纹理
    glGenTextures(1, &leftTexID);
    glBindTexture(GL_TEXTURE_2D, leftTexID);
    // 分配纹理内存（初始为空）
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 创建右眼纹理
    glGenTextures(1, &rightTexID);
    glBindTexture(GL_TEXTURE_2D, rightTexID);
    // 分配纹理内存（初始为空）
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return leftTexID; // 返回左纹理ID以保持兼容性
}

void GLDisplay::updateVideo(unsigned char* leftData, unsigned char* rightData, int width, int height) {
    // 纹理是共享的，只需要在任意一个有效上下文中更新一次
    // 使用第一个窗口的上下文
    if (windows.empty()) return;
    
    glfwMakeContextCurrent(windows[0]);
    
    // 更新左眼纹理数据
    glBindTexture(GL_TEXTURE_2D, leftTexID);
    // 使用glTexSubImage2D高效更新纹理（支持OpenCV的BGR格式）
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, leftData);

    // 更新右眼纹理数据
    glBindTexture(GL_TEXTURE_2D, rightTexID);
    // 使用glTexSubImage2D高效更新纹理（支持OpenCV的BGR格式）
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rightData);

    // 释放上下文，避免与渲染线程冲突
    glfwMakeContextCurrent(NULL);
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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, leftTexID);
    glUniform1i(texLeftLocation, 0);

    // 绑定右眼纹理到纹理单元1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rightTexID);
    glUniform1i(texRightLocation, 1);

    // 绘制全屏四边形（6个顶点）
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

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

    // 获取当前窗口的上下文
    glfwMakeContextCurrent(windows[windowIndex]);

    // 清除颜色缓冲区
    glClear(GL_COLOR_BUFFER_BIT);

    // 使用着色器程序
    glUseProgram(shaderProgram);
    // 绑定当前窗口的VAO
    glBindVertexArray(VAOs[windowIndex]);

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

    // 确保命令执行完成（关键：用于延迟测试）
    glFinish();

    // 交换前后缓冲区
    glfwSwapBuffers(windows[windowIndex]);

    // 释放上下文，允许其他线程使用
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

    // 主线程处理所有窗口的事件
    glfwPollEvents();
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
