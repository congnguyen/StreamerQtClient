#include "rtspparser.h"

//Gstreamer
GMainLoop *mainloop;
GstElement *pipeline;
GstElement *appsink;
std::queue<std::vector<uint8_t>> data_queue;
std::mutex queue_mutex;
std::condition_variable queue_condition;

struct MyGstContext {

};

RtspParser::~RtspParser()
{
    stop();
}

// Process nal units and save them
static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
    static int sample_index = 0;  // To keep track of sample numbers
    const std::string output_dir = "h264";

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


RtspParser::RtspParser(string rtspLink, bool loop, uint32_t samplesPerSecond)
{
    mRtspLink = rtspLink;
    this->loop = loop;
    this->sampleDuration_us = 1000 * 1000 / samplesPerSecond;
}


int RtspParser::startCapture()
{
    std::string pipeline_desc = "rtspsrc location=" + mRtspLink +  " latency=300 ! application/x-rtp,payload=96 ! "
                                "rtpvp8depay ! vp8dec ! videoconvert ! queue ! x264enc speed-preset=slow tune=zerolatency  ! appsink name=video_sink";

    std::cout << "Start pipeline: " << pipeline_desc << std::endl;
    GError *error = nullptr;
     pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (error) {
        std::cerr << "Error parsing pipeline: " << error->message << "\n";
        g_error_free(error);
        return -1;
    }

    // Get the appsink element
    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "video_sink");
    if (!appsink) {
        std::cerr << "Failed to get appsink element\n";
        gst_object_unref(pipeline);
        return -1;
    }

    // Configure appsink
    gst_app_sink_set_emit_signals((GstAppSink*)appsink, TRUE);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), NULL);

    // Start the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Main loop
    mainloop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(mainloop);

    return 0;
}

void RtspParser::start()
{
    mCaptureThread.dispatch([this](){
        std::cout << "Start Capture Media" << endl;
        this->startCapture();
    });
}

void RtspParser::stop()
{
    sample = {};
    sampleTime_us = 0;
    counter = -1;

    // Clean up
    g_main_loop_unref(mainloop);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsink);
    gst_object_unref(pipeline);
}

void RtspParser::loadNextSample()
{
//    if (loop && counter > 0) {
//        loopTimestampOffset = sampleTime_us;
//        counter = -1;
//        loadNextSample();
//        return;
//    }
    string frame_id = to_string(++counter);
    std::unique_lock<std::mutex> lock(queue_mutex);
    queue_condition.wait(lock, [] { return !data_queue.empty() ;});


    if (!data_queue.empty()) {
        vector<uint8_t> data = data_queue.front();
        auto *b = reinterpret_cast<const std::byte*>(data.data());
        sample.assign(b, b + data.size());
        std::cout << "Data frame: " << frame_id << " Size: " << data.size() << " Size1: " << sample.size() << std::endl;
        data_queue.pop();
        sampleTime_us += sampleDuration_us;
    }
}

uint64_t RtspParser::getSampleTime_us()
{
    return sampleTime_us;
}

uint64_t RtspParser::getSampleDuration_us()
{
    return sampleDuration_us;
}

rtc::binary RtspParser::getSample()
{
    return sample;
}
