#ifndef H_WLF_C5AA0CDA_9668_4C6C_B6F9_9EEFE7292C64
#define H_WLF_C5AA0CDA_9668_4C6C_B6F9_9EEFE7292C64
#include <thread>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <atomic> // 引入 atomic 头文件

class V4L2Capture;

class EndoViewer {
public:
    EndoViewer();
    ~EndoViewer();

    void startup(uint8_t left_cam_id = 0, uint8_t right_cam_id = 1, bool is_write_to_video = false);

    const uint16_t imwidth;
    const uint16_t imheight;
private:
    void readLeftImage(int index);
    void readRightImage(int index);
    void show(); // OpenCV can only show window in the same thread
    void writeVideo();

    std::thread _thread_read_l;
    std::thread _thread_read_r;

    V4L2Capture* _cap_l;
    V4L2Capture* _cap_r;

    cv::Mat _image_l;
    cv::Mat _image_r;

    bool _is_write_to_video;
    cv::VideoWriter  _writer;
    std::thread _thread_writer;

    // ⭐ 新增：用于控制线程退出的原子标志位
    std::atomic<bool> _keep_running;
};

#endif /* H_WLF_C5AA0CDA_9668_4C6C_B6F9_9EEFE7292C64 */
