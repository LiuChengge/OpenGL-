#include "endo_viewer.h"
#include <ctime>
#include <unistd.h>
#include "./inc/v4l2_capture.h"
#include "./inc/GLDisplay.h"

#define DO_EFFECIENCY_TEST 1

namespace {

    std::chrono::steady_clock::time_point getCurrentTimePoint() {
        return ::std::chrono::steady_clock::now();
    }

    long getDurationSince(const std::chrono::steady_clock::time_point &start_time_point)
	{
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		return ::std::chrono::duration_cast<
            std::chrono::milliseconds>(now - start_time_point).count();
    }

    inline ::std::string getCurrentTimeStr()
    {
        time_t timep;
        time(&timep);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "%Y%m%d_%H%M%S", localtime(&timep));

        return std::string(tmp);
    }

    const uint8_t TIME_INTTERVAL = 17;
}


EndoViewer::EndoViewer(bool simulate)
    : imwidth(1920), imheight(1080)
    , _image_l(cv::Mat(imheight, imwidth, CV_8UC3))
    , _image_r(cv::Mat(imheight, imwidth, CV_8UC3))
    , _is_simulated(simulate)
    , _glDisplay(nullptr)
    , _textureID(0)
    , _is_write_to_video(false)
{
    if (_is_simulated) {
        _glDisplay = new GLDisplay();
        if (!_glDisplay->init(800, 600, "Endoscope Viewer - Simulation Mode")) {
            printf("Failed to initialize GLDisplay\n");
            delete _glDisplay;
            _glDisplay = nullptr;
        } else {
            _textureID = _glDisplay->setupTexture(800, 600);
        }
    }
}


EndoViewer::~EndoViewer() {
    delete _cap_l;
    delete _cap_r;
    if (_glDisplay) {
        _glDisplay->cleanup();
        delete _glDisplay;
    }
    cv::destroyAllWindows();
}


void EndoViewer::startup(uint8_t left_cam_id, uint8_t right_cam_id, bool is_write_to_video) {
    _is_write_to_video = is_write_to_video;
    if(_is_write_to_video) {
        _thread_writer = std::thread(&EndoViewer::writeVideo, this);
        _thread_writer.detach();
    }

    // Skip camera initialization in simulation mode
    if (!_is_simulated) {
        _thread_read_l = std::thread(&EndoViewer::readLeftImage, this, left_cam_id);
        _thread_read_l.detach();
        _thread_read_r = std::thread(&EndoViewer::readRightImage, this, right_cam_id);
        _thread_read_r.detach();

        // Initialize GLDisplay for real camera latency testing
        if (!_glDisplay) {
            _glDisplay = new GLDisplay();
            if (!_glDisplay->init(1920, 1080, "Endoscope Viewer - Real Camera Mode")) {
                printf("Failed to initialize GLDisplay for real camera mode\n");
                delete _glDisplay;
                _glDisplay = nullptr;
            } else {
                _textureID = _glDisplay->setupTexture(imwidth, imheight);
            }
        }
    }

    show();
}


void EndoViewer::readLeftImage(int index) {
    _cap_l = new V4L2Capture(imwidth, imheight, 3);
    while(!_cap_l->openDevice(index)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        printf("Camera %d is retrying to connection!!!\n", index);
    }

    bool flag = 0;
    while(true) {
        auto time_start = ::getCurrentTimePoint();

        flag = _cap_l->ioctlDequeueBuffers(_image_l.data);
        flag = flag && (!_image_l.empty());

        // 在 readLeftImage 的 while 循环里，ioctlDequeueBuffers 之后，if(!flag) 之前：

        // --- 添加诊断代码 ---
        unsigned char* ptr = _image_l.data;
        printf("Data Check: [0]=%02X [1]=%02X Size=%ld\n", ptr[0], ptr[1], _image_l.total() * _image_l.elemSize());
        // ------------------

        if(!flag) {
            printf("EndoViewer::readLeftImage: USB ID: %d, image empty: %d.\n",
                    index, _image_l.empty());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto ms = getDurationSince(time_start);
#if DO_EFFECIENCY_TEST
        printf("EndoViewer::readLeftImage: [%ld]ms elapsed.\n", ms);
#endif
        if(ms < 17) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTTERVAL - ms));
        }
    }
}


void EndoViewer::readRightImage(int index) {
    _cap_r = new V4L2Capture(imwidth, imheight, 3);
    while(!_cap_r->openDevice(index)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        printf("Camera %d is retrying to connection!!!\n", index);
    }

    bool flag = 0;
    while(true) {
        auto time_start = ::getCurrentTimePoint();

        flag = _cap_r->ioctlDequeueBuffers(_image_r.data);
        flag = flag && (!_image_r.empty());
        if(!flag) {
            printf("EndoViewer::readRightImage: USB ID: %d, image empty: %d.\n",
                    index, _image_r.empty());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto ms = getDurationSince(time_start);
#if DO_EFFECIENCY_TEST
        printf("EndoViewer::readRightImage: [%ld]ms elapsed.\n", ms);
#endif
        if(ms < 17) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTTERVAL - ms));
        }
    }
}   


void EndoViewer::show() {
    if (_is_simulated) {
        if (!_glDisplay) {
            printf("GLDisplay not initialized for simulation mode\n");
            return;
        }
        showOpenGL();
    } else {
        // For Latency Test: Use OpenGL for real video instead of OpenCV
        if (!_glDisplay) {
            printf("GLDisplay not initialized for real camera mode\n");
            return;
        }
        showOpenGL();
    }
}


void EndoViewer::showOpenGL() {
    // ========== 初始化阶段 ==========
    if (_is_simulated) {
        printf("Starting OpenGL stereo simulation mode...\n");
    } else {
        printf("Starting OpenGL real camera latency test mode...\n");
    }

    // 验证GLDisplay是否已初始化
    if (!_glDisplay) {
        printf("ERROR: GLDisplay not initialized!\n");
        return;
    }

    if (_is_simulated) {
        // ========== 模拟模式：静态图像性能测试 ==========
        // 摄像头模拟参数
        const int camera_width = 640;
        const int camera_height = 480;

        // 加载 container.jpg 图像用于性能测试
        cv::Mat testImg = cv::imread("../container.jpg");
        if (testImg.empty()) {
            printf("Error: Failed to load container.jpg from project root\n");
            printf("Current working directory might be: ");
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                printf("unknown\n");
            }
            return;
        }

        // 调整图像大小以匹配摄像头分辨率
        cv::resize(testImg, testImg, cv::Size(camera_width, camera_height));

        // 确保图像格式为CV_8UC3
        if (testImg.channels() != 3) {
            cv::cvtColor(testImg, testImg, cv::COLOR_GRAY2BGR);
        }

        printf("Image loaded and resized: %dx%d, channels: %d\n",
               testImg.cols, testImg.rows, testImg.channels());

        int frame_count = 0;

        while (!_glDisplay->shouldClose()) {
            auto frame_start = ::getCurrentTimePoint();

            // 高精度计时器（用于精确延迟测量）
            auto timer_start = std::chrono::high_resolution_clock::now();

            // 更新双目纹理数据到GPU（使用相同的container.jpg图像）
            _glDisplay->updateVideo(testImg.data, testImg.data, camera_width, camera_height);

            auto timer_middle = std::chrono::high_resolution_clock::now();

            // 执行OpenGL渲染（着色器进行分屏拼接）
            _glDisplay->draw();

            // 强制GPU同步以准确测量渲染时间
            glFinish();
            auto timer_end = std::chrono::high_resolution_clock::now();

            frame_count++;

            // 计算精确的微秒级计时
            auto upload_duration = std::chrono::duration_cast<std::chrono::microseconds>(timer_middle - timer_start);
            auto render_duration = std::chrono::duration_cast<std::chrono::microseconds>(timer_end - timer_middle);

            // 性能监控输出
            if (frame_count % 60 == 0) {
                printf("Frame %d - Upload: %ld μs, Render+Finish: %ld μs, Total: %ld μs\n",
                       frame_count, upload_duration.count(), render_duration.count(),
                       (upload_duration + render_duration).count());
            }

            // 帧率控制（维持~60 FPS）
            auto frame_time = getDurationSince(frame_start);
            if (frame_time < TIME_INTTERVAL) {
                std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTTERVAL - frame_time));
            }
        }
    } else {
        // ========== 真实摄像头模式：低延迟渲染 ==========
        printf("Real camera latency test: consuming V4L2 camera feeds...\n");

        while (!_glDisplay->shouldClose()) {
            // 延迟测试：检查摄像头数据是否准备就绪
            if (_image_l.rows == 0 || _image_r.rows == 0) {
                // 摄像头还未准备好，跳过这一帧
                continue;
            }

            // 延迟测试关键：直接使用共享的cv::Mat数据
            // 不进行任何拷贝或转换，以最小化CPU处理延迟
            _glDisplay->updateVideo(_image_l.data, _image_r.data, imwidth, imheight);

            // 执行OpenGL渲染
            _glDisplay->draw();

            // 延迟测试：不添加睡眠，让渲染尽可能快
            // Dirty read是可以接受的，因为我们只关心延迟测试
        }
    }

    // ========== 清理阶段 ==========
    printf("EndoViewer: exit OpenGL mode.\n");
}


void EndoViewer::showOpenCV() {
    // Original OpenCV display mode
    cv::Mat bino;
    std::string win_name = "Bino";
    cv::namedWindow(win_name, cv::WINDOW_NORMAL);
    cv::moveWindow(win_name, 1920, 0);
    cv::setWindowProperty(win_name, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);

    std::string win_name2 = "Mono";
    cv::namedWindow(win_name2, cv::WINDOW_NORMAL);
    cv::moveWindow(win_name2, 0, 0);
    cv::setWindowProperty(win_name2, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);

    cv::Mat imleft, imright;
    bool is_show_left = true;
    while(true) {
        auto time_start = ::getCurrentTimePoint();

        cv::cvtColor(_image_l, imleft, cv::COLOR_RGB2BGR);
        cv::cvtColor(_image_r, imright, cv::COLOR_RGB2BGR);
        cv::hconcat(imleft, imright, bino);
        cv::imshow(win_name, bino);
        if(is_show_left) {
            cv::imshow(win_name2, imleft);
        }
        else {
            cv::imshow(win_name2, imright);
        }
        char key = cv::waitKey(10);
        if(key == 'q') {
            printf("EndoViewer: exit video showing.\n");
            break;
        }
        if(key == 'c') {
            is_show_left = !is_show_left;
        }

        auto ms = getDurationSince(time_start);
#if DO_EFFECIENCY_TEST
        printf("EndoViewer::showBino: [%ld]ms elapsed.\n", ms);
#endif
        if(ms < 17) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTTERVAL - ms));
        }
    }
}


void EndoViewer::writeVideo() {
    cv::Size size = cv::Size(imwidth * 2, imheight);
    _writer.open(getCurrentTimeStr() + ".avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, size, true);
    if (!_writer.isOpened()) {
        std::cout << "EndoViewer: cannot open the video writer!\n";
        std::exit(-1);
    }

    cv::Mat bino;
    bool is_show_left = true;
    auto time_org = ::getCurrentTimePoint();
    while(true) {
        auto time_start = ::getCurrentTimePoint();

        cv::hconcat(_image_l, _image_r, bino);
        _writer.write(bino);

        auto ms = getDurationSince(time_start);

        if(getDurationSince(time_org) > (60*1000)) {
            _writer.release();
            _writer.open(getCurrentTimeStr() + ".avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, size, true);
            time_org = ::getCurrentTimePoint();
        }
#if DO_EFFECIENCY_TEST
        printf("EndoViewer::writeVideo: [%ld]ms elapsed.\n", ms);
#endif

        if(ms < 17) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTTERVAL - ms));
        }
    }
}