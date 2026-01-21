#include "endo_viewer.h"
#include <ctime>
#include "./inc/v4l2_capture.h"
#include "./inc/GLDisplay.h"
#include "./inc/VkDisplay.h"

#define DO_EFFECIENCY_TEST 1  // 设置为1启用详细性能分析

// 渲染模式切换：0 = 串行渲染（单线程），1 = 并行渲染（多线程）
#define RENDER_MODE_PARALLEL 1

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

    // 新增：计算两个时间点之间的差值（微秒）
    inline long getDurationBetween(const std::chrono::steady_clock::time_point &start,
                                   const std::chrono::steady_clock::time_point &end)
    {
        return ::std::chrono::duration_cast<
            std::chrono::microseconds>(end - start).count();
    }

    const uint8_t TIME_INTTERVAL = 17;  // 17ms
}


EndoViewer::EndoViewer()
    : imwidth(1920), imheight(1080)
    , _is_write_to_video(false)
    , _keep_running(true)
{
    // 初始化双缓冲的所有 Mat
    for (int i = 0; i < 2; i++) {
        _image_l_buffers[i] = cv::Mat(imheight, imwidth, CV_8UC3);
        _image_r_buffers[i] = cv::Mat(imheight, imwidth, CV_8UC3);
    }
}


EndoViewer::~EndoViewer() {
    delete _cap_l;
    delete _cap_r;
    cv::destroyAllWindows();
}


void EndoViewer::startup(uint8_t left_cam_id, uint8_t right_cam_id, bool is_write_to_video) {
    _is_write_to_video = is_write_to_video;
    if(_is_write_to_video) {
        _thread_writer = std::thread(&EndoViewer::writeVideo, this);
        _thread_writer.detach();
    }

    _thread_read_l = std::thread(&EndoViewer::readLeftImage, this, left_cam_id);
    _thread_read_l.detach();
    _thread_read_r = std::thread(&EndoViewer::readRightImage, this, right_cam_id);
    _thread_read_r.detach();

    show();
}


void EndoViewer::readLeftImage(int index) {
    _cap_l = new V4L2Capture(imwidth, imheight, 3);
    while(!_cap_l->openDevice(index)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        printf("Camera %d is retrying to connection!!!\n", index);
    }

    bool flag = 0;
    while(_keep_running) {
        auto time_start = ::getCurrentTimePoint();

        // 获取当前写入索引
        int write_idx = _write_index_l.load(std::memory_order_relaxed);

        // 写入当前缓冲区
        flag = _cap_l->ioctlDequeueBuffers(_image_l_buffers[write_idx].data);
        flag = flag && (!_image_l_buffers[write_idx].empty());

        // // Debug: check data
        // if(flag) {
        //     unsigned char* ptr = _image_l_buffers[write_idx].data;
        //     printf("Data Check Left: [0]=%02X [1]=%02X Size=%ld\n", ptr[0], ptr[1], _image_l_buffers[write_idx].total() * _image_l_buffers[write_idx].elemSize());
        // }

        if(!flag) {
            printf("EndoViewer::readLeftImage: USB ID: %d, image empty: %d.\n",
                    index, _image_l_buffers[write_idx].empty());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // 写入完成后，切换缓冲区索引（原子操作，确保渲染线程看到完整帧）
        _write_index_l.store(1 - write_idx, std::memory_order_release);
        _new_frame_l.store(true, std::memory_order_release);

        auto ms = getDurationSince(time_start);
#if DO_EFFECIENCY_TEST
        printf("CAMERA_ACQUIRE: [%ld]ms\n", ms);
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
    while(_keep_running) {
        auto time_start = ::getCurrentTimePoint();

        int write_idx = _write_index_r.load(std::memory_order_relaxed);

        flag = _cap_r->ioctlDequeueBuffers(_image_r_buffers[write_idx].data);
        flag = flag && (!_image_r_buffers[write_idx].empty());

        // // Debug: check data
        // if(flag) {
        //     unsigned char* ptr = _image_r_buffers[write_idx].data;
        //     printf("Data Check Right: [0]=%02X [1]=%02X Size=%ld\n", ptr[0], ptr[1], _image_r_buffers[write_idx].total() * _image_r_buffers[write_idx].elemSize());
        // }

        if(!flag) {
            printf("EndoViewer::readRightImage: USB ID: %d, image empty: %d.\n",
                    index, _image_r_buffers[write_idx].empty());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        _write_index_r.store(1 - write_idx, std::memory_order_release);
        _new_frame_r.store(true, std::memory_order_release);

        auto ms = getDurationSince(time_start);
// #if DO_EFFECIENCY_TEST
//         printf("EndoViewer::readRightImage: [%ld]ms elapsed.\n", ms);
// #endif
        if(ms < 17) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_INTTERVAL - ms));
        }
    }
}


void EndoViewer::show() {
    printf("============================================================\n");
    printf("🚀 Starting Vulkan Low-Latency Mode (Mailbox Strategy)\n");
    printf("============================================================\n");

    // 1. 创建 Vulkan 显示实例
    VkDisplay* vkDisplay = new VkDisplay();
    
    // 2. 初始化 (注意：VkDisplay 内部已经封装了 GLFW 窗口创建)
    // 参数 2 是 dummy，因为 Vulkan 实现里不依赖这个数量，但为了兼容接口保留
    if (!vkDisplay->init(1920, 540, "Endoscope Viewer - Vulkan")) {
        printf("❌ Failed to initialize VkDisplay. Falling back or exiting.\n");
        delete vkDisplay;
        return;
    }

    printf("✅ Vulkan Initialized. Consuming camera feed...\n");

        // 3. 主循环
    while (!vkDisplay->shouldClose()) {
        auto frame_start = ::getCurrentTimePoint();

        // 处理窗口事件 (必须在主线程调用)
        vkDisplay->pollEvents();

        auto poll_end = ::getCurrentTimePoint();

        // 读取索引 = 1 - 写入索引（读取已完成写入的缓冲区）
        // 使用 memory_order_acquire 确保看到完整的帧数据
        int read_idx_l = 1 - _write_index_l.load(std::memory_order_acquire);
        int read_idx_r = 1 - _write_index_r.load(std::memory_order_acquire);

        // 检查缓冲区是否有效
        if (_image_l_buffers[read_idx_l].empty() ||
            _image_r_buffers[read_idx_r].empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        auto buffer_check_end = ::getCurrentTimePoint();

        // 4. 数据上传 (CPU -> Staging Buffer)
        // Vulkan 的 updateVideo 只是内存拷贝 (memcpy)，非常快
        vkDisplay->updateVideo(
            _image_l_buffers[read_idx_l].data,
            _image_r_buffers[read_idx_r].data,
            imwidth, imheight
        );

        auto upload_end = ::getCurrentTimePoint();

        // 5. 渲染提交 (Submit & Present)
        // 这一步是非阻塞的，除非 GPU 积压了超过 MAX_FRAMES_IN_FLIGHT 帧
        vkDisplay->draw();

        auto draw_end = ::getCurrentTimePoint();

#if DO_EFFECIENCY_TEST
        printf("FRAME_PROFILE: poll=%ld us, buffer_check=%ld us, upload=%ld us, draw=%ld us, total=%ld us\n",
               getDurationBetween(frame_start, poll_end),
               getDurationBetween(poll_end, buffer_check_end),
               getDurationBetween(buffer_check_end, upload_end),
               getDurationBetween(upload_end, draw_end),
               getDurationBetween(frame_start, draw_end));
#endif
    }

    printf("EndoViewer: exit Vulkan mode.\n");

    _keep_running = false;
    // 稍微等待一下，让子线程安全退出（可选，防止析构过快）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    vkDisplay->cleanup();
    delete vkDisplay;

//     // OpenGL low-latency display mode for camera latency testing
//     // Replace original OpenCV display with OpenGL for minimal latency
//     printf("Starting OpenGL real camera latency test mode...\n");

//     // Initialize OpenGL display with 2 windows for latency testing
//     GLDisplay* glDisplay = new GLDisplay();
//     if (!glDisplay->init(1920, 540, "Endoscope Viewer - OpenGL Mode", 2)) {
//         printf("Failed to initialize GLDisplay\n");
//         delete glDisplay;
//         return;
//     }

//     if (!glDisplay->setupTexture(imwidth, imheight)) {
//         printf("Failed to setup GLDisplay texture\n");
//         delete glDisplay;
//         return;
//     }

//     // 打印当前使用的渲染模式（VSync开启）
//     printf("Real camera latency test: consuming V4L2 camera feeds...\n");
// #if RENDER_MODE_PARALLEL
//     printf("*** RENDERING MODE: PARALLEL + VSync (Interval 1) ***\n");
// #else
//     printf("*** RENDERING MODE: SERIAL + VSync (Interval 1) ***\n");
// #endif

//     // Main display loop - no frame rate limiting for latency testing
//     while (!glDisplay->shouldClose()) {
//         // Check if camera data is ready
//         if (_image_l.rows == 0 || _image_r.rows == 0) {
//             continue;
//         }

//         // Direct OpenGL rendering without data copying for minimum latency
//         glDisplay->updateVideo(_image_l.data, _image_r.data, imwidth, imheight);
        
//         // 根据宏选择渲染模式
// #if RENDER_MODE_PARALLEL
//         glDisplay->drawParallel();
// #else
//         glDisplay->drawSerial();
// #endif
//     }

//     printf("EndoViewer: exit OpenGL latency test mode.\n");
//     glDisplay->cleanup();
//     delete glDisplay;
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
    while(_keep_running) {  // 使用 _keep_running 而不是 while(true)
        auto time_start = ::getCurrentTimePoint();

        // 使用双缓冲的读取索引
        int read_idx_l = 1 - _write_index_l.load(std::memory_order_acquire);
        int read_idx_r = 1 - _write_index_r.load(std::memory_order_acquire);

        cv::hconcat(_image_l_buffers[read_idx_l],
                    _image_r_buffers[read_idx_r], bino);
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