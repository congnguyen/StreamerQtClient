#ifndef OPUSRTSPPARSER_H
#define OPUSRTSPPARSER_H

#include "rtspparser.h"
#include "dispatchqueue.hpp"

class opusrtspparser : public StreamSource
{
    static const uint32_t defaultSamplesPerSecond = 50;
    // for player
    uint64_t sampleDuration_us;
    uint64_t sampleTime_us = 0;
    uint32_t counter = -1;
    bool loop;
    uint64_t loopTimestampOffset = 0;
    //Rtsp Link
    string mRtspLink;
    // frame payload
    rtc::binary sample = {};

public:
    ~opusrtspparser();
    opusrtspparser(string rtspLink, bool loop, uint32_t samplesPerSecond = opusrtspparser::defaultSamplesPerSecond);

    virtual void start() override;
    virtual void stop() override;
    virtual void loadNextSample() override;
    virtual uint64_t getSampleTime_us() override;
    virtual uint64_t getSampleDuration_us() override;
    virtual rtc::binary getSample() override;
private:
    int startCapture();
    DispatchQueue mCaptureThread = DispatchQueue("CaptureThread");
};

#endif // OPUSRTSPPARSER_H
