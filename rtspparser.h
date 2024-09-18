#ifndef RTSPPARSER_H
#define RTSPPARSER_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <iostream>

#include "stream.hpp"

using namespace std;

class RtspParser : public StreamSource
{
    uint64_t sampleDuration_us;
    uint64_t sampleTime_us = 0;
    uint32_t counter = -1;
    bool loop;
    uint64_t loopTimestampOffset = 0;
    string mRtspLink;

protected:
    rtc::binary sample = {};
public:
    ~RtspParser();
    RtspParser(string rtspLink, bool loop, uint32_t samplesPerSecond);
    int startCapture();
    DispatchQueue mCaptureThread = DispatchQueue("CaptureThread");

    // StreamSource interface
public:
    virtual void start() override;
    virtual void stop() override;
    virtual void loadNextSample() override;
    virtual uint64_t getSampleTime_us() override;
    virtual uint64_t getSampleDuration_us() override;
    virtual rtc::binary getSample() override;
};

#endif // RTSPPARSER_H
