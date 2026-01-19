#include "inc/GLDisplay.h"
#include <iostream>
#include <stb_image.h>

GLDisplay::GLDisplay() : window(nullptr), shaderProgram(0), VAO(0), VBO(0), windowWidth(0), windowHeight(0) {
}

GLDisplay::~GLDisplay() {
    cleanup();
}

bool GLDisplay::init(int width, int height, std::string title) {
    windowWidth = width;
    windowHeight = height;

    // 初始化GLFW窗口系统
    if (!initGLFW(width, height, title)) {
        return false;
    }

    // 初始化GLAD OpenGL函数加载器
    if (!initGLAD()) {
        return false;
    }

    // 编译和链接GLSL着色器程序
    if (!compileShaders()) {
        return false;
    }

    // 设置全屏四边形顶点数据
    setupQuad();

    // 设置背景清除颜色为黑色
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return true;
}

bool GLDisplay::initGLFW(int width, int height, std::string title) {
    // 初始化GLFW库
    if (!glfwInit()) {
        return false;
    }

    // 配置OpenGL上下文版本和核心模式
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);  // 使窗口可见（用于性能测试）

    
    // 创建GLFW窗口
    window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    if (!window) {
        glfwTerminate();
        return false;
    }

    // 设置当前OpenGL上下文
    glfwMakeContextCurrent(window);

    // 禁用垂直同步（消除卡顿）
    glfwSwapInterval(0);
    
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

    return true;
}

void GLDisplay::setupQuad() {
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

    unsigned int EBO;
    // 生成顶点数组对象、顶点缓冲对象和索引缓冲对象
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // 绑定VAO
    glBindVertexArray(VAO);

    // 设置顶点缓冲数据
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 设置索引缓冲数据
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标属性 (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑缓冲区
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    // 更新左眼纹理数据
    glBindTexture(GL_TEXTURE_2D, leftTexID);
    // 使用glTexSubImage2D高效更新纹理（支持OpenCV的BGR格式）
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, leftData);

    // 更新右眼纹理数据
    glBindTexture(GL_TEXTURE_2D, rightTexID);
    // 使用glTexSubImage2D高效更新纹理（支持OpenCV的BGR格式）
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rightData);
}

void GLDisplay::draw() {
    // 清除颜色缓冲区
    glClear(GL_COLOR_BUFFER_BIT);

    // 使用着色器程序
    glUseProgram(shaderProgram);
    // 绑定顶点数组对象
    glBindVertexArray(VAO);

    // 绑定左眼纹理到纹理单元0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, leftTexID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texLeft"), 0);

    // 绑定右眼纹理到纹理单元1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rightTexID);
    glUniform1i(glGetUniformLocation(shaderProgram, "texRight"), 1);

    // 绘制全屏四边形（6个顶点）
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    //glFlush();
    glFinish();

    // 交换前后缓冲区并处理事件
    glfwSwapBuffers(window);
    glfwPollEvents();
}

bool GLDisplay::shouldClose() {
    return glfwWindowShouldClose(window);
}

void GLDisplay::cleanup() {
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (shaderProgram) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}
