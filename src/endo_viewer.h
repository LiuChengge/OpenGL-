#ifndef H_WLF_C5AA0CDA_9668_4C6C_B6F9_9EEFE7292C64
#define H_WLF_C5AA0CDA_9668_4C6C_B6F9_9EEFE7292C64

#include <thread>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <atomic>

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
    void show();
    void writeVideo();

    std::thread _thread_read_l;
    std::thread _thread_read_r;
    V4L2Capture* _cap_l;
    V4L2Capture* _cap_r;

    // ========== 双缓冲系统 ==========
    // 每个相机使用两个缓冲区，采集线程和渲染线程各自操作不同的缓冲区
    cv::Mat _image_l_buffers[2];
    cv::Mat _image_r_buffers[2];

    // 指示当前采集线程正在写入的缓冲区索引 (0 或 1)
    // 渲染线程读取的是另一个缓冲区 (1 - _write_index)
    std::atomic<int> _write_index_l{0};
    std::atomic<int> _write_index_r{0};

    // 帧 ID 追踪（用于最新帧策略）
    std::atomic<uint64_t> _frame_id_l{0};
    std::atomic<uint64_t> _frame_id_r{0};

    // 标记是否有新帧就绪（可选，用于跳帧检测）
    std::atomic<bool> _new_frame_l{false};
    std::atomic<bool> _new_frame_r{false};
    // ================================

    bool _is_write_to_video;
    cv::VideoWriter _writer;
    std::thread _thread_writer;
    std::atomic<bool> _keep_running;
};

#endif /* H_WLF_C5AA0CDA_9668_4C6C_B6F9_9EEFE7292C64 */
