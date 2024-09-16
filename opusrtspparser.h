#ifndef OPUSRTSPPARSER_H
#define OPUSRTSPPARSER_H

#include "rtspparser.h"

class opusrtspparser : public RtspParser
{
    static const uint32_t defaultSamplesPerSecond = 50;
public:
    opusrtspparser(string rtspLink, bool loop, uint32_t samplesPerSecond = opusrtspparser::defaultSamplesPerSecond);
};

#endif // OPUSRTSPPARSER_H
