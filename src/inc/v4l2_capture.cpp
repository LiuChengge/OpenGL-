#include "v4l2_capture.h"
#include "mjpeg2jpeg.h"
#include <poll.h>
// #include "/opt/libjpeg-turbo/include/turbojpeg.h"  // Temporarily disabled for OpenGL testing
#include <iostream>
#include <cstring>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define MJPG_FORMAT_VALUE 1196444237
#define YUYV_FORMAT_VALUE 1448695129

/*
VIDIOC_QUERYCAP     check the properties of the device
VIDIOC_ENUM_FMT     the format of the frame
VIDIOC_S_FMT        set the format of the frame (struct v4l2_format)
VIDIOC_G_FMT        get the format of the frame
VIDIOC_REQBUFS      apply several buffer for frame, no less than 3 buffer
VIDIOC_QUERYBUF     查询帧缓冲区在内核空间的长度和偏移量
VIDIOC_QBUF         将申请到的帧缓冲区全部放入视频采集输出队列
VIDIOC_STREAMON     start capture video streaming
VIDIOC_DQBUF        应用程序从视频采集输出队列中取出已含有采集数据的帧缓冲区
VIDIOC_STREAMOFF    应用程序将该帧缓冲区重新挂入输入队列
 */

namespace
{
    void errno_exit(const char *err)
    {
        std::cout << err << " error" << errno << ", " << strerror(errno) << std::endl;
        // exit(EXIT_FAILURE);
    }

    int xioctl(int fh, unsigned long int request, void *arg)
    {
        int r;
        do{
            r = ioctl(fh, request, arg);
        }while( r == -1 && EINTR == errno); // meets error or interrupted system call
        return r;
    }

    static bool V4L2Capture_deviceHandlePoll(const std::vector<int> &device_handles, std::vector<int> &ready, int64_t timeout_ms)
    {
        const size_t N = device_handles.size();

        ready.clear();
        ready.reserve(N);

        const auto poll_flags = POLLIN | POLLRDNORM | POLLERR;

        std::vector<pollfd> fds;
        fds.reserve(N);

        for(size_t i = 0; i < N; i++)
        {
            int hd = device_handles[i];
            fds.push_back(pollfd{hd, poll_flags, 0});
        }

        int64_t timeoutMs = -1;
        if(timeout_ms > 0)
            timeoutMs = timeout_ms;

        int ret = poll(fds.data(), N, timeoutMs);
        if(ret == -1)
            return false;
        if(ret == 0)
            return 0;
        for(size_t i = 0; i < N; ++i)
        {
            const auto &fd = fds[i];
            if((fd.revents & (POLLIN | POLLRDNORM)) != 0 )
                ready.push_back(i);
            else if((fd.revents & POLLERR) != 0)
                std::cout << "Error is reported for camera stream: " << i << ", handle " << device_handles[i] << std::endl;
            else {
                // not ready
            }
        }
        return true;
    }
}

V4L2Capture::V4L2Capture(uint width, uint height, uint buffer_count/* = 3 */)
    : cameraFd(-1)
    , buffer_mmap_ptr(nullptr)
    , decode_buffer(nullptr)
    , buffer_count(buffer_count)
    , frame_width(width)
    , frame_height(height)
    , fps(60)
    , sharpness(3)
{
    decode_buffer = new uchar[frame_width * frame_height * 3];
    jpeg_buffer = new uchar[frame_width * frame_height * 3];
    CLEAR(device_name);
}

V4L2Capture::~V4L2Capture()
{
    closeDevice();
    delete [] decode_buffer;
    delete [] jpeg_buffer;
}

bool V4L2Capture::openDevice(int index)
{
    if(index < 0){
        //qDebug() << "device should not less than 0, current index: " << index;
        return false;
    }
    CLEAR(device_name);
    sprintf(device_name, "/dev/video%d", index);
    return initDevice(device_name);
}

bool V4L2Capture::initDevice(const char* device_name)
{
    std::lock_guard<std::mutex> lck(mtx);
    if(!openDevice(device_name))
        return false;

    // check the basis information (selected do)
    ioctlQueryCapability();
    if(!ioctlEnumFmt())
        return false;

    // set some parameters and format for capturing (must do)
    ioctlSetSharpnessParm();
    ioctlSetStreamParm();
    ioctlSetStreamFmt();
    // aplly buffer room and do the process (must do)
    ioctlRequestBuffers();
    ioctlMmapBuffers();
    ioctlQueueBuffers();
    ioctlSetStreamSwitch(true);

    return true;
}

bool V4L2Capture::openDevice(const char* device_name)
{
    struct stat st;

    // Get file attributes for 'device_name' and put them in 'st'.
    if(stat(device_name, &st) == -1)
    {
        std::cout << "Cannot identify device: " << device_name << ", " << errno << ", " << strerror(errno) << std::endl;
        return false;
    }

    // make sure it is a device
    if(!S_ISCHR(st.st_mode))
    {
        std::cout << device_name << " is not a device" << std::endl;
        return false;
    }

    resetDevice();
    // open the device and return a new file descriptor for it, or -1 on error.
    cameraFd = open(device_name, O_RDWR, 0); // | O_NONBLOCK
    if(cameraFd == -1)
    {
        std::cout<< "Cannot open device: " << device_name << ", " << errno << ", " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void V4L2Capture::closeDevice()
{
    if(close(cameraFd) == -1)
        std::cout << "Close camera device failed" << std::endl;
    else cameraFd = -1;
}


void V4L2Capture::ioctlQueryCapability()
{
    v4l2_capability cap;
    if(xioctl(cameraFd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        if(errno == EINVAL)
        {
            std::cout << "No valid V4L2 device exist.\n";
            // exit(EXIT_FAILURE);
        }
        else {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        std::cout << "No video capture device.\n";
        // exit(EXIT_FAILURE);
    }

    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        std::cout << "Does not support streaming.\n";
        // exit(EXIT_FAILURE);
    }
}

void V4L2Capture::ioctlQueryStd()
{
    v4l2_std_id std; // represented by V4L2_STD_* micro
    ioctl(cameraFd, VIDIOC_QUERYSTD, &std);
    //qDebug() << "Analog video standard: " << std;
}

bool V4L2Capture::ioctlEnumFmt()
{
    /*struct v4l2_fmtdesc {
    __u32		    index;             // Format number
    __u32		    type;              // enum v4l2_buf_type
    __u32           flags;
    __u8		    description[32];   // Description string
    __u32		    pixelformat;       // Format fourcc, MJPG, YUYV,...
    __u32		    reserved[4];
    };*/

    v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    bool support_mjpg = false;
    // display all the supported format
    std::cout << "Support Format: \n";
    while(ioctl(cameraFd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        std::cout << "flags=" << fmtdesc.flags << "\tdescription=" << fmtdesc.description << "\tpixel format=" << char(fmtdesc.pixelformat&0xFF) << char((fmtdesc.pixelformat>>8)&0xFF) << char((fmtdesc.pixelformat>>16)&0xFF) << char((fmtdesc.pixelformat>>24)&0xFF) << std::endl;
        fmtdesc.index++;

        support_mjpg = support_mjpg || (fmtdesc.pixelformat == MJPG_FORMAT_VALUE);
    }
    return support_mjpg;
}

void V4L2Capture::ioctlSetStreamParm()
{
    v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // capture mode: when a qualitfied image is required, mode set to 1, otherwise set to 0
    streamparm.parm.capture.capturemode = 0;

    /* set the fps. The parameters set here is a nominal value, the actual fps is depend on
       the device */
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = this->fps;
    ioctl(cameraFd, VIDIOC_S_PARM, &streamparm);

    // get the set fps
    ioctl(cameraFd, VIDIOC_G_PARM, &streamparm);
    std::cout << "Capture Mode: " << streamparm.parm.capture.capturemode << "\nFrame Rate: " << streamparm.parm.capture.timeperframe.numerator << "/" << streamparm.parm.capture.timeperframe.denominator << std::endl;
}

void V4L2Capture::ioctlSetStreamFmt()
{
    v4l2_format format;
    CLEAR(format);

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(xioctl(cameraFd, VIDIOC_G_FMT, &format) == -1)
        errno_exit("VIDIOC_G_FMT");
    if(format.fmt.pix.pixelformat != V4L2_PIX_FMT_JPEG)
        std::cout << "Not using MJPEG as data format\n";

    // set/change the format
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = frame_width;        // a value should be divide by 16
    format.fmt.pix.height = frame_height;      // a value should be divide by 16
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_ANY; // not sure whether need to set

    if(xioctl(cameraFd, VIDIOC_S_FMT, &format) == -1)
        errno_exit("VIDIOC_S_FMT");
    if(format.fmt.pix.width != frame_width)
        std::cout << "Device reset width to " << frame_width << std::endl;
    if(format.fmt.pix.height != frame_height)
        std::cout << "Device reset height to " << frame_height << std::endl;
    if(format.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG)
        errno_exit("VIDIOC_S_FMT: Unable to set V4L2_PIX_FMT_MJPEG");
}

void V4L2Capture::ioctlSetSharpnessParm()
{
    v4l2_control ctrl = v4l2_control();
    ctrl.id = V4L2_CID_SHARPNESS;
    ctrl.value = static_cast<int>(this->sharpness);
    tryIoctl(VIDIOC_S_CTRL, &ctrl);
}

void V4L2Capture::ioctlRequestBuffers()
{
    v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = buffer_count;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP; // set the way for mapping, which will reduce the time of data copy

    if(xioctl(cameraFd, VIDIOC_REQBUFS, &reqbufs) == -1)
    {
        if(errno == EINVAL)
        {
                std::cout << "Does not support memoru mapping\n";
                //exit(EXIT_FAILURE);
        }
        else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }
}

void V4L2Capture::ioctlMmapBuffers()
{
    buffer_mmap_ptr = new bufferMmap[buffer_count];
    v4l2_buffer vbuffer;
    for(uint i = 0; i < buffer_count; i++)
    {
        CLEAR(vbuffer);
        vbuffer.index = i;
        vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuffer.memory = V4L2_MEMORY_MMAP;
        // check buffer info of i-th frame, including addr and length
        if(xioctl(cameraFd, VIDIOC_QUERYBUF, &vbuffer))
            errno_exit("VIDIOC_QUERYBUF");

        // store the length and address of the buffer frame
        buffer_mmap_ptr[i].length = vbuffer.length;
        buffer_mmap_ptr[i].addr = mmap(nullptr, vbuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, cameraFd, vbuffer.m.offset);
        if(buffer_mmap_ptr[i].addr == MAP_FAILED)
            errno_exit("mmap");
    }
}

void V4L2Capture::ioctlQueueBuffers()
{
    v4l2_buffer vbuffer;
    for(uint i = 0; i < buffer_count; i++)
    {
        CLEAR(vbuffer);
        vbuffer.index = i;
        vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuffer.memory = V4L2_MEMORY_MMAP;
        if(xioctl(cameraFd, VIDIOC_QBUF, &vbuffer) == -1)
            errno_exit("VIDIOC_QBUF");
    }
}

bool V4L2Capture::ioctlDequeueBuffers(unsigned char* data)
{
    std::lock_guard<std::mutex> lck(mtx);
    if(cameraFd < 0)
        return false;

    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(cameraFd, &fds);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int err = 0;
    errno = 0;

#if 0
    struct timespec before, after;
    long elapsed_nsecs;
    clock_gettime(CLOCK_REALTIME, &before);
#endif
    int r = select(cameraFd + 1, &fds, nullptr, nullptr, &tv);
#if 0
    clock_gettime(CLOCK_REALTIME, &after);
    elapsed_nsecs = (after.tv_sec - before.tv_sec) * 1e9 + (after.tv_nsec - before.tv_nsec);
    qDebug() << "Device name: " << device_name << ", select time: " << elapsed_nsecs << " ns\n";
#endif

    err = errno;

    if(0 == r)
    {
        std::cout << "device name: " << device_name << ", select timeout!\n";
        return false;
    }
    else if(r == -1)
    {
        std::cout << "device name: " << device_name << ", result: " << r << ", errno: " << err << ", error info: " << strerror(err);
        return false;
    }

    if(EINTR == err)
        return false;

    v4l2_buffer vbuffer;
    CLEAR(vbuffer);
    vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuffer.memory = V4L2_MEMORY_MMAP;

    // output a buffer frame from video streaming
    if(xioctl(cameraFd, VIDIOC_DQBUF, &vbuffer) == -1)
    {
        switch(errno)
        {
        case EAGAIN:
        case EIO:
        default:
            errno_exit("VIDIOC_DQBUF");
            return false;
        }
    }

    bool decompress_mjpeg_success = false;
    if(vbuffer.length > 0)
    {
        //GET_CURRENT_TIME(start);
        decompress_mjpeg_success = processImage(buffer_mmap_ptr[vbuffer.index].addr, vbuffer.length, data);

        //GET_CURRENT_TIME(end);
        //decompress_time = ::std::chrono::duration_cast<::std::chrono::milliseconds>(end - start).count();
    }

    // put the buffer room back to queue to achieve a loop for data capturing
    if(xioctl(cameraFd, VIDIOC_QBUF, &vbuffer) == -1)
        errno_exit("VIDIOC_DBUF");

    return decompress_mjpeg_success;
}

void V4L2Capture::ioctlSetStreamSwitch(bool on)
{
    v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(on)
    {
        // start the capturing stream
        if(xioctl(cameraFd, VIDIOC_STREAMON, &type) == -1)
            errno_exit("VIDIOC_STREAMON");
    }
    else {
        if(xioctl(cameraFd, VIDIOC_STREAMOFF, &type) == -1)
            errno_exit("VIDIOC_STREAMOFF");
    }
}

void V4L2Capture::unMmapBuffers()
{
    for(uint i = 0; i < buffer_count; i++)
        munmap(buffer_mmap_ptr[i].addr, buffer_mmap_ptr[i].length);
}

bool V4L2Capture::processImage(const void *p, uint size, unsigned char* data)
{
    unsigned int jpg_size = 0;

    bool bSuccess = mjpeg2jpeg(static_cast<const byte*>(p), size, jpeg_buffer, frame_width*frame_height * 3, &jpg_size);

    if(!bSuccess)
    {
        std::cout << "mjpeg2jpeg failed!\n";
        return false;
    }
    bSuccess = decodeJPEG(jpeg_buffer, jpg_size);
    if(bSuccess)
    {
        memcpy(data, decode_buffer, frame_height*frame_width*3*sizeof(unsigned char));
    }
    else std::cout << "Jpeg decompression failed!\n";

    return bSuccess;
}


bool V4L2Capture::waitAny(const std::vector<int> &camera_fds, std::vector<int> &ready_index, int64_t timeout_ms)
{
    return V4L2Capture_deviceHandlePoll(camera_fds, ready_index, timeout_ms);
}

bool V4L2Capture::decodeJPEG(uchar *pcompressed_image, unsigned long jpeg_size)
{
    // Temporarily disabled for OpenGL testing
    // TODO: Re-enable when turbojpeg is properly configured
    // For now, just return true to avoid compilation errors
    return true;
}

void V4L2Capture::resetDevice()
{
    if(cameraFd != -1)
    {
        ioctlSetStreamSwitch(false);
        unMmapBuffers();
        delete [] buffer_mmap_ptr;
        closeDevice();
        CLEAR(device_name);
    }
}

bool V4L2Capture::tryIoctl(unsigned long ioctl_code, void *param, bool fail_if_busy, int attempts) const
{
    while(true)
    {
        errno = 0;
        int res = ioctl(cameraFd, ioctl_code, param);
        int err = errno;
        if(res != -1)
            return true;

        const bool is_busy = (err == EBUSY);
        if(is_busy && fail_if_busy)
            return false;
        if(!(is_busy || errno == EAGAIN))
            return false;

        if(--attempts == 0)
            return false;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(cameraFd, &fds);

        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        errno = 0;
        res = select(cameraFd + 1, &fds, nullptr, nullptr, &tv);
        err = errno;

        if(res == 0)
            return false;
        if(EINTR == err)    // stop loop onec signal appears, such as Ctrl+C
            return false;
    }
    return true;
}

