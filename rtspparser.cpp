#include "rtspparser.h"

//Gstreamer

struct VGstContext {
    GMainLoop *mainloop;
    GstElement *pipeline;
    GstElement *vappsink;

};

struct AGstContext {
    GMainLoop *mainloop;
    GstElement *pipeline;
    GstElement *aappsink;
    std::queue<std::vector<uint8_t>> a_data_queue;
    std::mutex a_queue_mutex;
    std::condition_variable a_queue_condition;
};

VGstContext *mVGstContext = nullptr;
AGstContext *mAGstContext = nullptr;

RtspParser::~RtspParser()
{
    stop();
}

// Process nal units for audio and save them
static GstFlowReturn on_new_audio_sample(GstAppSink *sink, gpointer user_data) {
    VGstContext *vContext = static_cast<VGstContext*>(user_data);
    if (!vContext) {
        std::cerr << "VGstContext is null" << std::endl;
        return GST_FLOW_ERROR;
    }
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

//        // Push to queue
//        std::lock_guard<std::mutex> lock(vContext->v_queue_mutex);

//        vContext->v_data_queue.push(std::move(final_data));
//        vContext->v_queue_condition.notify_one();
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// Process nal units and save them
static GstFlowReturn on_new_video_sample(GstAppSink *sink, gpointer user_data) {
    VGstContext *vContext = static_cast<VGstContext*>(user_data);
    if (!vContext) {
        std::cerr << "VGstContext is null" << std::endl;
        return GST_FLOW_ERROR;
    }
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
//        std::lock_guard<std::mutex> lock(vContext->v_queue_mutex);

//        vContext->v_data_queue.push(std::move(final_data));
//        vContext->v_queue_condition.notify_one();
//        gst_buffer_unmap(buffer, &map);
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

//gst-launch-1.0 -v rtspsrc location=rtsp://10.8.0.84/sample.webm latency=200 name=src \
//     src. ! queue ! rtpvp8depay ! vp8dec ! videoconvert ! x264enc tune=zerolatency bitrate=500  ! queue !  appsink name=videosink \
//     src. ! queue ! rtpvorbisdepay ! vorbisdec ! audioconvert ! audioresample ! opusenc bitrate=64000 ! queue  ! appsink name=audiosink

int RtspParser::startCapture()
{
    std::string pipeline_desc = "rtspsrc location=" + mRtspLink +  " latency=300  name=src !"
                                " src. ! queue ! rtpvp8depay ! vp8dec ! videoconvert ! x264enc tune=zerolatency bitrate=500  ! queue !  appsink name=videosink"
                                " src. ! queue ! rtpvorbisdepay ! vorbisdec ! audioconvert ! audioresample ! opusenc bitrate=64000 ! queue  ! appsink name=audiosink";

    std::cout << "Start pipeline: " << pipeline_desc << std::endl;
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (error) {
        std::cerr << "Error parsing pipeline: " << error->message << "\n";
        g_error_free(error);
        return -1;
    }

    //Init VGstContext and AGstContext

    mVGstContext = static_cast<VGstContext*>(g_malloc(sizeof(VGstContext)));
    mAGstContext = static_cast<AGstContext*>(g_malloc(sizeof(AGstContext)));

    // Get the video appsink element
    GstElement *vappsink = gst_bin_get_by_name(GST_BIN(pipeline), "videosink");
    if (!vappsink) {
        std::cerr << "Failed to get video appsink element\n";
        gst_object_unref(pipeline);
        return -1;
    }

    // Get the audio appsink element
    GstElement *aappsink = gst_bin_get_by_name(GST_BIN(pipeline), "audiosink");
    if (!aappsink) {
        std::cerr << "Failed to get audio appsink element\n";
        gst_object_unref(pipeline);
        return -1;
    }
    // Main loop
    GMainLoop *mainloop = g_main_loop_new(NULL, FALSE);
    mVGstContext->pipeline = pipeline;
    mVGstContext->vappsink = vappsink;
    mVGstContext->mainloop = mainloop;

    mAGstContext->pipeline = pipeline;
    mAGstContext->aappsink = aappsink;
    mAGstContext->mainloop = mainloop;

    // Configure video appsink
    gst_app_sink_set_emit_signals((GstAppSink*)vappsink, TRUE);
    g_signal_connect(vappsink, "new-sample", G_CALLBACK(on_new_video_sample), mVGstContext);

    // Configure audio appsink
    gst_app_sink_set_emit_signals((GstAppSink*)aappsink, TRUE);
    g_signal_connect(aappsink, "new-sample", G_CALLBACK(on_new_audio_sample), mAGstContext);

    // Start the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
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
    if (mVGstContext) {
        if (mVGstContext->mainloop) {
             g_main_loop_unref(mVGstContext->mainloop);
        }
        if (mVGstContext->pipeline) {
             gst_element_set_state(mVGstContext->pipeline, GST_STATE_NULL);
             gst_object_unref(mVGstContext->pipeline);
        }
        if (mVGstContext->vappsink) {
             gst_object_unref(mVGstContext->vappsink);
        }
    }

    if (mAGstContext) {
        if (mAGstContext->mainloop) {
             g_main_loop_unref(mAGstContext->mainloop);
        }
        if (mAGstContext->pipeline) {
             gst_element_set_state(mAGstContext->pipeline, GST_STATE_NULL);
             gst_object_unref(mAGstContext->pipeline);
        }
        if (mAGstContext->aappsink) {
             gst_object_unref(mAGstContext->aappsink);
        }
    }
}

void RtspParser::loadNextSample()
{
//    if (loop && counter > 0) {
//        loopTimestampOffset = sampleTime_us;
//        counter = -1;
//        loadNextSample();
//        return;
//    }
//    string frame_id = to_string(++counter);
//    std::cout << "Load Data frame: " << frame_id
//    std::unique_lock<std::mutex> lock(queue_mutex);
//    queue_condition.wait(lock, [] { return !data_queue.empty() ;});


//    if (!data_queue.empty()) {
//        vector<uint8_t> data = data_queue.front();
//        auto *b = reinterpret_cast<const std::byte*>(data.data());
//        sample.assign(b, b + data.size());
//        std::cout << "Data frame: " << frame_id << " Size: " << data.size() << " Size1: " << sample.size() << std::endl;
//        data_queue.pop();
//        sampleTime_us += sampleDuration_us;
//    }
}

uint64_t RtspParser::getSampleTime_us()
{
    return sampleTime_us;
}

uint64_t RtspParser::getSampleDuration_us()
{
    return sampleTime_us;
}

rtc::binary RtspParser::getSample()
{
    return sample;
}
