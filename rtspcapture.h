#ifndef RTSPCAPTURE_H
#define RTSPCAPTURE_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <iostream>

class RtspCapture
{
public:
    RtspCapture();
    int startCapture();

};

#endif // RTSPCAPTURE_H
