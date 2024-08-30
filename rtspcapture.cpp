#include "rtspcapture.h"

RtspCapture::RtspCapture()
{

}

int RtspCapture::startCapture()
{
    const char* rtsp_url = "rtsp://10.8.0.82/sample.webm";

    // Initialize all components
    // av_register_all();
    avformat_network_init();

    AVFormatContext* formatContext = avformat_alloc_context();

    // Open the RTSP stream
    if (avformat_open_input(&formatContext, rtsp_url, nullptr, nullptr) != 0) {
        std::cerr << "Could not open input stream." << std::endl;
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream info." << std::endl;
        return -1;
    }

    // Find the first video and audio stream
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
        } else if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
        }
    }

    if (videoStreamIndex == -1 || audioStreamIndex == -1) {
        std::cerr << "Could not find video or audio stream." << std::endl;
        return -1;
    }

    // Prepare video codec
    const AVCodec* videoCodec = avcodec_find_decoder(formatContext->streams[videoStreamIndex]->codecpar->codec_id);
    AVCodecContext* videoCodecContext = avcodec_alloc_context3(videoCodec);
    avcodec_parameters_to_context(videoCodecContext, formatContext->streams[videoStreamIndex]->codecpar);
    avcodec_open2(videoCodecContext, videoCodec, nullptr);

    // Prepare audio codec
    const AVCodec* audioCodec = avcodec_find_decoder(formatContext->streams[audioStreamIndex]->codecpar->codec_id);
    AVCodecContext* audioCodecContext = avcodec_alloc_context3(audioCodec);
    avcodec_parameters_to_context(audioCodecContext, formatContext->streams[audioStreamIndex]->codecpar);
    avcodec_open2(audioCodecContext, audioCodec, nullptr);

    // Prepare H.264 encoder
    const AVCodec* h264Codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext* h264CodecContext = avcodec_alloc_context3(h264Codec);
    h264CodecContext->bit_rate = 400000;
    h264CodecContext->width = videoCodecContext->width;
    h264CodecContext->height = videoCodecContext->height;
    h264CodecContext->time_base = (AVRational){1, 25};
    h264CodecContext->framerate = (AVRational){25, 1};
    h264CodecContext->gop_size = 10;
    h264CodecContext->max_b_frames = 1;
    h264CodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    avcodec_open2(h264CodecContext, h264Codec, nullptr);

    // Prepare Opus encoder
    const AVCodec* opusCodec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    AVCodecContext* opusCodecContext = avcodec_alloc_context3(opusCodec);
    opusCodecContext->sample_rate = 48000; // standard Opus sample rate
    opusCodecContext->channels = 2;
    opusCodecContext->channel_layout = av_get_default_channel_layout(2);
    opusCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP   ;
    opusCodecContext->bit_rate = 64000;
    opusCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    avcodec_open2(opusCodecContext, opusCodec, nullptr);

    // Setup resampler
    SwrContext* swrContext = swr_alloc_set_opts(
        nullptr,
        opusCodecContext->channel_layout,
        opusCodecContext->sample_fmt,
        opusCodecContext->sample_rate,
        audioCodecContext->channel_layout,
        audioCodecContext->sample_fmt,
        audioCodecContext->sample_rate,
        0,
        nullptr
        );
    swr_init(swrContext);

    // Allocate frames

    // Allocate frames and packets
    AVFrame* frame = av_frame_alloc();
    AVFrame* resampledFrame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();

    // Set up output files
    FILE* videoOutputFile = fopen("out.264", "wb");
    FILE* audioOutputFile = fopen("out.opus", "wb");

    // Initialize resampled frame properties
    resampledFrame->channel_layout = opusCodecContext->channel_layout;
    resampledFrame->format = opusCodecContext->sample_fmt;
    resampledFrame->sample_rate = opusCodecContext->sample_rate;
    resampledFrame->nb_samples = opusCodecContext->frame_size;
    av_frame_get_buffer(resampledFrame, 0);

    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            // Decode video frame
            if (avcodec_send_packet(videoCodecContext, packet) >= 0) {
                while (avcodec_receive_frame(videoCodecContext, frame) >= 0) {
                    // Encode to H.264
                    if (avcodec_send_frame(h264CodecContext, frame) >= 0) {
                        AVPacket* h264Packet = av_packet_alloc();
                        while (avcodec_receive_packet(h264CodecContext, h264Packet) >= 0) {
                            fwrite(h264Packet->data, 1, h264Packet->size, videoOutputFile);
                            av_packet_unref(h264Packet);
                        }
                        av_packet_free(&h264Packet);
                    }
                }
            }
        } else if (packet->stream_index == audioStreamIndex) {
            // Decode audio frame
            if (avcodec_send_packet(audioCodecContext, packet) >= 0) {
                while (avcodec_receive_frame(audioCodecContext, frame) >= 0) {
                    // Resample and encode to Opus
                    swr_convert(swrContext, resampledFrame->data, resampledFrame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);

                    if (avcodec_send_frame(opusCodecContext, resampledFrame) >= 0) {
                        AVPacket* opusPacket = av_packet_alloc();
                        while (avcodec_receive_packet(opusCodecContext, opusPacket) >= 0) {
                            printf("Opus frame size: %d\n", opusPacket->size);
                            fwrite(opusPacket->data, 1, opusPacket->size, audioOutputFile);
                            av_packet_unref(opusPacket);
                        }
                        av_packet_free(&opusPacket);
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    // Flush encoders
    avcodec_send_frame(h264CodecContext, nullptr);
    while (avcodec_receive_packet(h264CodecContext, packet) >= 0) {
        fwrite(packet->data, 1, packet->size, videoOutputFile);
        av_packet_unref(packet);
    }

    avcodec_send_frame(opusCodecContext, nullptr);
    while (avcodec_receive_packet(opusCodecContext, packet) >= 0) {
        fwrite(packet->data, 1, packet->size, audioOutputFile);
        av_packet_unref(packet);
    }

    // Clean up
    fclose(videoOutputFile);
    fclose(audioOutputFile);

    av_frame_free(&frame);
    av_frame_free(&resampledFrame);
    av_packet_free(&packet);

    avcodec_free_context(&videoCodecContext);
    avcodec_free_context(&audioCodecContext);
    avcodec_free_context(&h264CodecContext);
    avcodec_free_context(&opusCodecContext);

    swr_free(&swrContext);
    avformat_close_input(&formatContext);
    avformat_network_deinit();
}
