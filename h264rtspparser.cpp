#include "h264rtspparser.h"

h264RtspParser::h264RtspParser(string rtspLink, uint32_t fps, bool loop) :
    RtspParser(rtspLink, loop, fps)
{

}

void h264RtspParser::loadNextSample()
{
    RtspParser::loadNextSample();

    size_t i = 0;
    while (i < sample.size()) {
        assert(i + 4 < sample.size());
        auto lengthPtr = (uint32_t *) (sample.data() + i);
        uint32_t length;
        std::memcpy(&length, lengthPtr, sizeof(uint32_t));
        length = ntohl(length);
        auto naluStartIndex = i + 4;
        auto naluEndIndex = naluStartIndex + length;
        assert(naluEndIndex <= sample.size());
        auto header = reinterpret_cast<rtc::NalUnitHeader *>(sample.data() + naluStartIndex);
        auto type = header->unitType();
        switch (type) {
        case 7:
            previousUnitType7 = {sample.begin() + i, sample.begin() + naluEndIndex};
            break;
        case 8:
            previousUnitType8 = {sample.begin() + i, sample.begin() + naluEndIndex};;
            break;
        case 5:
            previousUnitType5 = {sample.begin() + i, sample.begin() + naluEndIndex};;
            break;
        }
        i = naluEndIndex;
    }
}

std::vector<std::byte> h264RtspParser::initialNALUS()
{
    vector<std::byte> units{};
    if (previousUnitType7.has_value()) {
        auto nalu = previousUnitType7.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (previousUnitType8.has_value()) {
        auto nalu = previousUnitType8.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (previousUnitType5.has_value()) {
        auto nalu = previousUnitType5.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    return units;
}
