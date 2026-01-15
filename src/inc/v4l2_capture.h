#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/videodev2.h> // the header of class V4L2
#include <memory>
#include <vector>
#include <mutex>


/** @brief This class is designed for video capture.
 * In details, this modules depends on the data structure and interface of V4L2.
 */
class V4L2Capture
{
    using uint = unsigned int;
    using uchar = unsigned char;
public:
    /* Functions relevent to video capture */
    explicit V4L2Capture(uint width, uint height, uint buffer_count = 3);
    ~V4L2Capture();

    /** @brief Open the video device
     * @param filename  the name of the device (should be a absolute path, /dev/video1)
     * @return true/false
     */
    bool openDevice(int index);
private:
    /** @brief Initializing the device before video capture
     */
    bool initDevice(const char* device_name);

    bool openDevice(const char* device_name);
    /** @brief Close the video device
     */
    void closeDevice();

    /* The functions belows utlizes the data structure of class V4L2 to read, write and control
       the media device.
       ioctl() functions are used for read/write or control devices.
       When the ioctl() interfaces were called in user's workspace, the cammands to tell the
       driver is needed only, and the driver will make the implementations. (The V4L2 is the
       driver in the case).
    */
    /** @brief Check the basic information and V4L2_capability of the devices
     * For a general device, its capability just can support video capture
     * (V4L2_CAP_VIDEO_CAPTURE) and ioctl (V4L2_CAP_STREAMING) control generally.
     */
    void ioctlQueryCapability();

    /** @brief Check the supported digital video standard (v4l2_std_id) of the device, such as
     * PAL/NTSC.
     */
    void ioctlQueryStd();

    /** @brief Display the supported frame format (v4l2_fmtdesc)
     * In this fucntion, it checks the supported capturing frame format.
     */
    bool ioctlEnumFmt();

    /** @brief Set the parameters (v4l2_streamparm) of video streaming.
     * In this function, the capture mode and fps of the input video streaming are set.
     * Note, this process is time-consuming. However, if unset the parameters, it might cannot
     * capture normal image.
     */
    void ioctlSetStreamParm();

    /** @brief Set the format (v4l2_streamparm) of input video streaming
     */
    void ioctlSetStreamFmt();

    /** @brief Set the sharpness (v4l2_control) of the camera device
     */
    void ioctlSetSharpnessParm();

    /** @brief Apply buffer room (v4l2_requestbuffers) for video streaming (generic space)
     */
    void ioctlRequestBuffers();

    /** @brief Map the video buffer (v4l2_buffer) to user's workspace memory
     */
    void ioctlMmapBuffers();

    /** @brief Add the video buffer frame into queue
     * The drivee will store each captured frame data into buffer room of the queue, and then
     * move the buffer room to the output queue of video capture streaming automatically.
     */
    void ioctlQueueBuffers();
public:
    /** @brief Get frame from output queue
     */
    bool ioctlDequeueBuffers(unsigned char* data);
private:
    /** @brief Start/stop video capture
     */
    void ioctlSetStreamSwitch(bool on);

    /** @brief Release the mapping memory in buffer room
     */
    void unMmapBuffers();


    bool processImage(const void *p, uint size, unsigned char* data);

    static bool waitAny(const std::vector<int>& camera_fds, std::vector<int> &ready_index, int64_t timeout_ms);

    int getFd() { return this->cameraFd; }


    void setSharpness(uint value)
    {
        this->sharpness = value;
        if(cameraFd > 0)
            ioctlSetSharpnessParm();
    }

    void setFPS(uint value)
    {
        this->fps = value;
        initDevice(device_name);
    }

private:
    bool decodeJPEG(uchar* pcompressed_image, long unsigned int jpeg_size);
    void resetDevice();
    bool tryIoctl(unsigned long ioctl_code, void *param, bool fail_if_busy = true, int attempts = 10) const;

private:
    int     cameraFd;       // handle of device

    /* This structure store the info of each buffer frame mapped into memory */
    typedef struct bufferMmap{
        void* addr;     // the start address in memory when buffer frame mapped into memory
        uint  length;   // the length in the memory
    }bufferMmap;
    bufferMmap  *buffer_mmap_ptr;

    char    device_name[256];
    uchar  *decode_buffer;
    uchar  *jpeg_buffer;
    uint    buffer_count;
    uint    frame_width;
    uint    frame_height;
    uint    fps;
    uint    sharpness;

    std::mutex      mtx;
};
#endif  // V4L2_CAPTURE_H
