#include "opusrtspparser.h"

struct AGstContext {
    GMainLoop *mainloop;
    GstElement *pipeline;
    GstElement *appsink;
};
// Constants for Ogg sync code
const uint8_t OGGS_SYNC_CODE[4] = {0x4F, 0x67, 0x67, 0x53};

AGstContext *mAGstCtx = nullptr;
std::queue<std::vector<uint8_t>> a_data_queue;
std::mutex a_queue_mutex;
std::condition_variable a_queue_condition;

// Process nal units for audio and save them
static GstFlowReturn on_new_audio_sample(GstAppSink *sink, gpointer user_data) {
    mAGstCtx = static_cast<AGstContext*>(user_data);
    if (!mAGstCtx) {
        std::cerr << "AGstContext is nullptr " << std::endl;
        return GST_FLOW_ERROR;
    }
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        std::cerr << "Failed to pull sample from appsink.\n";
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        const uint8_t *data = map.data;
        gsize size = map.size;
        std::vector<uint8_t> segment_data; // fixed size 161 bytes
        segment_data.reserve(size);
        segment_data.insert(segment_data.end(), data, data + size);
        // Push to queue
        std::lock_guard<std::mutex> lock(a_queue_mutex);

        a_data_queue.push(std::move(segment_data));
        a_queue_condition.notify_one();
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
    return GST_FLOW_OK;
}

opusrtspparser::~opusrtspparser()
{
    stop();
}

opusrtspparser::opusrtspparser(string rtspLink, bool loop, uint32_t samplesPerSecond)
{
    mRtspLink = rtspLink;
    this->loop = loop;
    this->sampleDuration_us = 1000 * 1000 / samplesPerSecond;
}

void opusrtspparser::start()
{
    //Init  AGstContext
    mAGstCtx = static_cast<AGstContext*>(g_malloc(sizeof(AGstContext)));
    mCaptureThread.dispatch([this](){
        std::cout << "Start Capture Audio Thread" << endl;
        this->startCapture();
    });
}

void opusrtspparser::stop()
{
    sample = {};
    sampleTime_us = 0;
    counter = -1;

    // Clean up
    if (mAGstCtx) {
        if (mAGstCtx->mainloop) {
            g_main_loop_unref(mAGstCtx->mainloop);
        }
        if (mAGstCtx->pipeline) {
            gst_element_set_state(mAGstCtx->pipeline, GST_STATE_NULL);
            gst_object_unref(mAGstCtx->pipeline);
        }
        if (mAGstCtx->appsink) {
            gst_object_unref(mAGstCtx->appsink);
        }
    }
}

void opusrtspparser::loadNextSample()
{
    string frame_id = to_string(++counter);
    std::unique_lock<std::mutex> lock(a_queue_mutex);
    a_queue_condition.wait(lock, [this] { return ! a_data_queue.empty() ;});

    if (!a_data_queue.empty()) {
        vector<uint8_t> data = a_data_queue.front();
        auto *b = reinterpret_cast<const std::byte*>(data.data());
        sample.assign(b, b + data.size());
//        std::cout << "Audio Data frame: " << frame_id << " Size: " << data.size() << " Size1: " << sample.size() << std::endl;
        a_data_queue.pop();
        sampleTime_us += sampleDuration_us;
    }
}

uint64_t opusrtspparser::getSampleTime_us()
{
    return sampleTime_us;
}

uint64_t opusrtspparser::getSampleDuration_us()
{
    return sampleDuration_us;
}

rtc::binary opusrtspparser::getSample()
{
    return sample;
}

int opusrtspparser::startCapture()
{
    std::string pipeline_desc = "rtspsrc location=" + mRtspLink + " latency=1000 "
    " ! queue ! rtpopusdepay ! opusparse ! appsink name=audiosink";

    std::cout << "Start pipeline: " << pipeline_desc << std::endl;
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (error) {
        std::cerr << "Error parsing pipeline: " << error->message << "\n";
        g_error_free(error);
        return -1;
    }

    //Init AGstContext
    mAGstCtx = static_cast<AGstContext*>(g_malloc(sizeof(AGstContext)));

    // Get the audio appsink element
    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "audiosink");
    if (!appsink) {
        std::cerr << "Failed to get audio appsink element\n";
        gst_object_unref(pipeline);
        return -1;
    }
    // Main loop
    GMainLoop *mainloop = g_main_loop_new(NULL, FALSE);

    mAGstCtx->pipeline = pipeline;
    mAGstCtx->appsink = appsink;
    mAGstCtx->mainloop = mainloop;

    // Configure audio appsink
    gst_app_sink_set_emit_signals((GstAppSink*)appsink, TRUE);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_audio_sample), mAGstCtx);

    // Start the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(mainloop);

    return 0;
}
