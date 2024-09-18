#ifndef H264RTSPPARSER_H
#define H264RTSPPARSER_H

#include "rtspparser.h"
#include "dispatchqueue.hpp"

using namespace std;



class h264RtspParser : public StreamSource
{
private:
    //NAL Unit (pps,sps, keyframe)
    std::optional<std::vector<std::byte>> previousUnitType5 = std::nullopt;
    std::optional<std::vector<std::byte>> previousUnitType7 = std::nullopt;
    std::optional<std::vector<std::byte>> previousUnitType8 = std::nullopt;
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
    ~h264RtspParser();
    h264RtspParser(string rtspLink, uint32_t fps, bool loop);
    std::vector<std::byte> initialNALUS();
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

#endif // H264RTSPPARSER_H
