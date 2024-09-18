#include "h264rtspparser.h"

struct VGstContext {
    GMainLoop *mainloop;
    GstElement *pipeline;
    GstElement *vappsink;
};
VGstContext *mVGstCtx = nullptr;
std::queue<std::vector<uint8_t>> data_queue;
std::mutex queue_mutex;
std::condition_variable queue_condition;

h264RtspParser::~h264RtspParser()
{
    stop();
}

h264RtspParser::h264RtspParser(string rtspLink, uint32_t fps, bool loop)
{
    this->mRtspLink = rtspLink;
    this->loop = loop;
    this->sampleDuration_us = 1000 * 1000 / fps;
}

void h264RtspParser::loadNextSample()
{
    string frame_id = to_string(++counter);
    std::unique_lock<std::mutex> lock(queue_mutex);
    queue_condition.wait(lock, [this] { return ! data_queue.empty() ;});

    if (!data_queue.empty()) {
        vector<uint8_t> data = data_queue.front();
        auto *b = reinterpret_cast<const std::byte*>(data.data());
        sample.assign(b, b + data.size());
        std::cout << "Video Data frame: " << frame_id << " Size: " << data.size() << " Size1: " << sample.size() << std::endl;
        data_queue.pop();
        sampleTime_us += sampleDuration_us;
    }

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

uint64_t h264RtspParser::getSampleTime_us()
{
    return sampleTime_us;
}

uint64_t h264RtspParser::getSampleDuration_us()
{
    return sampleDuration_us;
}

rtc::binary h264RtspParser::getSample()
{
    return sample;
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

void h264RtspParser::start()
{
    //Init VGstContext
    mVGstCtx = static_cast<VGstContext*>(g_malloc(sizeof(VGstContext)));
    mCaptureThread.dispatch([this](){
        std::cout << "Start Capture Video Thread" << endl;
        this->startCapture();
    });
}

void h264RtspParser::stop()
{
    sample = {};
    sampleTime_us = 0;
    counter = -1;

    // Clean up
    if (mVGstCtx) {
        if (mVGstCtx->mainloop) {
            g_main_loop_unref(mVGstCtx->mainloop);
        }
        if (mVGstCtx->pipeline) {
            gst_element_set_state(mVGstCtx->pipeline, GST_STATE_NULL);
            gst_object_unref(mVGstCtx->pipeline);
        }
        if (mVGstCtx->vappsink) {
            gst_object_unref(mVGstCtx->vappsink);
        }
    }
}

// Process nal units and save them
static GstFlowReturn on_new_video_sample(GstAppSink *sink, gpointer user_data) {
    mVGstCtx = static_cast<VGstContext*>(user_data);
    if (!mVGstCtx) {
        std::cerr << "VGstContext is null" << std::endl;
        return GST_FLOW_ERROR;
    }
    static int sample_index = 0;  // To keep track of sample numbers

    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        std::cerr << "Failed to get sample from appsink.\n";
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        const uint8_t *data = map.data;
        gsize size = map.size;
        std::vector<uint8_t> final_data;
        size_t pos = 0;

        while (pos < size) {
            // Search for the next start code
            size_t next_start_code = size;
            for (size_t i = pos; i < size - 3; ++i) {
                if (data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
                    next_start_code = i;
                    break;
                }
                if (i < size - 4 && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
                    next_start_code = i;
                    break;
                }
            }
            // Determine the length of the NAL unit
            if (next_start_code > pos) {
                size_t nal_length = next_start_code - pos;
                uint32_t nal_length_net = htonl(static_cast<uint32_t>(nal_length));

                // Append length and NAL unit to final data
                final_data.insert(final_data.end(), (uint8_t*)&nal_length_net, (uint8_t*)&nal_length_net + 4);
                final_data.insert(final_data.end(), data + pos, data + next_start_code);
            }
            // Skip the start code
            if (next_start_code < size && data[next_start_code + 2] == 0x01) {
                pos = next_start_code + 3;
            } else if (next_start_code < size) {
                pos = next_start_code + 4;
            } else {
                break; // End of data
            }
        }

        // Handle the last NAL unit if no more start codes are found
        if (pos < size) {
            size_t nal_length = size - pos;
            uint32_t nal_length_net = htonl(static_cast<uint32_t>(nal_length));
            final_data.insert(final_data.end(), (uint8_t*)&nal_length_net, (uint8_t*)&nal_length_net + 4);
            final_data.insert(final_data.end(), data + pos, data + size);
        }

        // Push to queue
        std::lock_guard<std::mutex> lock(queue_mutex);

        data_queue.push(std::move(final_data));
        queue_condition.notify_one();
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

int h264RtspParser::startCapture()
{
    std::string pipeline_desc = "rtspsrc location=" + mRtspLink +  " latency=100 !"
    " queue ! rtpvp8depay ! vp8dec ! videoconvert ! x264enc tune=zerolatency bitrate=500  ! queue !  appsink name=videosink";

    std::cout << "Start pipeline: " << pipeline_desc << std::endl;
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (error) {
        std::cerr << "Error parsing pipeline: " << error->message << "\n";
        g_error_free(error);
        return -1;
    }

    // Get the video appsink element
    GstElement *vappsink = gst_bin_get_by_name(GST_BIN(pipeline), "videosink");
    if (!vappsink) {
        std::cerr << "Failed to get video appsink element\n";
        gst_object_unref(pipeline);
        return -1;
    }

    // Main loop
    GMainLoop *mainloop = g_main_loop_new(NULL, FALSE);
    mVGstCtx->pipeline = pipeline;
    mVGstCtx->vappsink = vappsink;
    mVGstCtx->mainloop = mainloop;

    // Configure video appsink
    gst_app_sink_set_emit_signals((GstAppSink*)vappsink, TRUE);
    g_signal_connect(vappsink, "new-sample", G_CALLBACK(on_new_video_sample), mVGstCtx);

    // Start the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(mainloop);

    return 0;
}
