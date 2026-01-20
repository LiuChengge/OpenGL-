#include "endo_viewer.h"
#include <ctime>
#include "./inc/v4l2_capture.h"
#include "./inc/GLDisplay.h"

#define DO_EFFECIENCY_TEST 1

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

    const uint8_t TIME_INTTERVAL = 17;
}


EndoViewer::EndoViewer()
    : imwidth(1920), imheight(1080)
    , _image_l(cv::Mat(imheight, imwidth, CV_8UC3))
    , _image_r(cv::Mat(imheight, imwidth, CV_8UC3))
    , _is_write_to_video(false)
{
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
    while(true) {
        auto time_start = ::getCurrentTimePoint();

        flag = _cap_l->ioctlDequeueBuffers(_image_l.data);
        flag = flag && (!_image_l.empty());

        // Debug: check data
        if(flag) {
            unsigned char* ptr = _image_l.data;
            printf("Data Check Left: [0]=%02X [1]=%02X Size=%ld\n", ptr[0], ptr[1], _image_l.total() * _image_l.elemSize());
        }

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

        // Debug: check data
        if(flag) {
            unsigned char* ptr = _image_r.data;
            printf("Data Check Right: [0]=%02X [1]=%02X Size=%ld\n", ptr[0], ptr[1], _image_r.total() * _image_r.elemSize());
        }

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
    // OpenGL low-latency display mode for camera latency testing
    // Replace original OpenCV display with OpenGL for minimal latency
    printf("Starting OpenGL real camera latency test mode...\n");

    // Initialize OpenGL display with 2 windows for latency testing
    GLDisplay* glDisplay = new GLDisplay();
    if (!glDisplay->init(1920, 540, "Endoscope Viewer - OpenGL Mode", 2)) {
        printf("Failed to initialize GLDisplay\n");
        delete glDisplay;
        return;
    }

    if (!glDisplay->setupTexture(imwidth, imheight)) {
        printf("Failed to setup GLDisplay texture\n");
        delete glDisplay;
        return;
    }

    // 打印当前使用的渲染模式（VSync开启）
    printf("Real camera latency test: consuming V4L2 camera feeds...\n");
#if RENDER_MODE_PARALLEL
    printf("*** RENDERING MODE: PARALLEL + VSync (Interval 1) ***\n");
#else
    printf("*** RENDERING MODE: SERIAL + VSync (Interval 1) ***\n");
#endif

    // Main display loop - no frame rate limiting for latency testing
    while (!glDisplay->shouldClose()) {
        // Check if camera data is ready
        if (_image_l.rows == 0 || _image_r.rows == 0) {
            continue;
        }

        // Direct OpenGL rendering without data copying for minimum latency
        glDisplay->updateVideo(_image_l.data, _image_r.data, imwidth, imheight);
        
        // 根据宏选择渲染模式
#if RENDER_MODE_PARALLEL
        glDisplay->drawParallel();
#else
        glDisplay->drawSerial();
#endif
    }

    printf("EndoViewer: exit OpenGL latency test mode.\n");
    glDisplay->cleanup();
    delete glDisplay;
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