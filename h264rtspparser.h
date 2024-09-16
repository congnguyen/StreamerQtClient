#ifndef H264RTSPPARSER_H
#define H264RTSPPARSER_H

#include "rtspparser.h"
#include "dispatchqueue.hpp"

using namespace std;

class h264RtspParser : public RtspParser
{
    std::optional<std::vector<std::byte>> previousUnitType5 = std::nullopt;
    std::optional<std::vector<std::byte>> previousUnitType7 = std::nullopt;
    std::optional<std::vector<std::byte>> previousUnitType8 = std::nullopt;

public:
    h264RtspParser(string rtspLink, uint32_t fps, bool loop);
    void loadNextSample() override;
    std::vector<std::byte> initialNALUS();

};

#endif // H264RTSPPARSER_H
