extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
};

#define SOKOL_IMPL
#define SOKOL_NO_ENTRY
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_audio.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include <iostream>
#include <chrono>

sg_pass_action pass_action{};
sg_buffer vbuf{};
sg_buffer ibuf{};
sg_pipeline pip{};
sg_bindings bind{};

const uint32_t SCR_WIDTH = 1280;
const uint32_t SCR_HEIGHT = 720;

struct FFmpegState {
    AVFormatContext* pFormatCtx = nullptr;
    int videoStream = -1;
    int audioStream = -1;

    const AVCodec* pCodec = nullptr;
    AVCodecContext* pCodecCtx = nullptr;

    const AVCodec* aCodec = nullptr;
    AVCodecContext* aCodecCtx = nullptr;

    AVFrame* pFrame = nullptr;
    AVFrame* pFrameRGB = nullptr;
    AVFrame* aFrame = nullptr;
    AVFrame* audioframe = nullptr;

    uint8_t* buffer = nullptr;

    SwsContext* sws_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;

    AVPacket* pPacket = nullptr;

    int frameIndex = 0;
};
FFmpegState ffmpeg_state{};

void printHelpMenu()
{
    printf("Invalid arguments.\n\n");
    printf("Usage: ./tutorial03 <filename>\n\n");
    printf("e.g: ./tutorial03 /home/rambodrahmani/Videos/Labrinth-Jealous.mp4\n");
}

void init() {
    sg_desc desc = {};
    desc.context = sapp_sgcontext();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    saudio_desc as_desc = {};
    as_desc.logger.func = slog_func;
    as_desc.buffer_frames = 1024;
    as_desc.sample_rate = 44110;
    // as_desc.num_channels = 2;
    saudio_setup(&as_desc);
    assert(saudio_channels() == 1);

    const float vertices[] = {
            // positions     uv
            -1.0, -1.0, 0.0, 0.0, 1.0,
            1.0,  -1.0, 0.0, 1.0, 1.0,
            1.0,  1.0,  0.0, 1.0, 0.0,
            -1.0, 1.0,  0.0, 0.0, 0.0,
    };
    sg_buffer_desc vb_desc = {};
    vb_desc.data = SG_RANGE(vertices);
    vbuf = sg_make_buffer(&vb_desc);

    const int indices[] = { 0, 1, 2, 0, 2, 3, };
    sg_buffer_desc ib_desc = {};
    ib_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ib_desc.data = SG_RANGE(indices);
    ibuf = sg_make_buffer(&ib_desc);

    sg_shader_desc shd_desc = {};
    shd_desc.attrs[0].name = "position";
    shd_desc.attrs[1].name = "texcoord0";
    shd_desc.vs.source = R"(
#version 330
layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord0;
out vec4 color;
out vec2 uv;
void main() {
  gl_Position = vec4(position, 1.0f);
  uv = texcoord0;
  color = vec4(uv, 0.0f, 1.0f);
}
)";
    shd_desc.fs.images[0].name = "tex";
    shd_desc.fs.images[0].image_type = SG_IMAGETYPE_2D;
    shd_desc.fs.images[0].sampler_type = SG_SAMPLERTYPE_FLOAT;
    shd_desc.fs.source = R"(
#version 330
uniform sampler2D tex;
in vec4 color;
in vec2 uv;
out vec4 frag_color;
void main() {
  frag_color = texture(tex, uv);
  //frag_color = pow(frag_color, vec4(1.0f/2.2f));
}
)";

    sg_image_desc img_desc = {};
    img_desc.width = SCR_WIDTH;
    img_desc.height = SCR_HEIGHT;
    img_desc.label = "nes-texture";
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.usage = SG_USAGE_STREAM;

    sg_shader shd = sg_make_shader(&shd_desc);

    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = shd;
    pip_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.index_type = SG_INDEXTYPE_UINT32;
    pip = sg_make_pipeline(&pip_desc);

    bind.vertex_buffers[0] = vbuf;
    bind.index_buffer = ibuf;
    bind.fs_images[0] = sg_make_image(&img_desc);

    pass_action.colors[0] = { .action=SG_ACTION_CLEAR, .value={0.5f, 0.0f, 0.0f, 1.0f} };


}

void frame() {
    const double dt = sapp_frame_duration();

    if (av_read_frame(ffmpeg_state.pFormatCtx, ffmpeg_state.pPacket) >= 0) {
        if (ffmpeg_state.pPacket->stream_index == ffmpeg_state.videoStream) {
            int ret = avcodec_send_packet(ffmpeg_state.pCodecCtx, ffmpeg_state.pPacket);
            if (ret < 0) {
                printf("Error sending packet for decoding.\n");
                exit(-1);
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(ffmpeg_state.pCodecCtx, ffmpeg_state.pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    // EOF exit loop
                    break;
                } else if (ret < 0) {
                    printf("Error while decoding.\n");
                    exit(-1);
                }

                sws_scale(ffmpeg_state.sws_ctx, ffmpeg_state.pFrame->data, ffmpeg_state.pFrame->linesize, 0, ffmpeg_state.pCodecCtx->height, ffmpeg_state.pFrameRGB->data, ffmpeg_state.pFrameRGB->linesize);

                {
                    typedef std::chrono::high_resolution_clock clock;
                    typedef std::chrono::duration<float, std::milli> duration;

                    clock::time_point start = clock::now();

                    printf(
                        "Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
                        av_get_picture_type_char(ffmpeg_state.pFrame->pict_type),
                        ffmpeg_state.pCodecCtx->frame_number,
                        ffmpeg_state.pFrame->pts,
                        ffmpeg_state.pFrame->pkt_dts,
                        ffmpeg_state.pFrame->key_frame,
                        ffmpeg_state.pFrame->coded_picture_number,
                        ffmpeg_state.pFrame->display_picture_number,
                        ffmpeg_state.pCodecCtx->width,
                        ffmpeg_state.pCodecCtx->height
                    );

                    sg_image_data image_data{};
                    image_data.subimage[0][0] = { .ptr=ffmpeg_state.pFrameRGB->data[0], .size=(SCR_WIDTH * SCR_HEIGHT * sizeof(uint32_t)) };
                    sg_update_image(bind.fs_images[0], image_data);

                    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
                    sg_apply_pipeline(pip);
                    sg_apply_bindings(&bind);
                    sg_draw(0, 6, 1);
                    sg_end_pass();
                    sg_commit();

                    clock::time_point end = clock::now();
                    duration elapsed = end - start;
                    double diffms = elapsed.count();

                    double fps = av_q2d(ffmpeg_state.pFormatCtx->streams[ffmpeg_state.videoStream]->r_frame_rate);
                    double sleep_time_ms = 1.0/(double)fps * 1000;

                    if (diffms < sleep_time_ms ) {
                        uint32_t diff = (uint32_t)((sleep_time_ms - diffms));
                        printf("diffms: %f, delay time %d ms.\n", diffms, diff);
                        // std::this_thread::sleep_for(std::chrono::milliseconds(diff));
                        Sleep(diff);
                    }
                }
            }

        }

        if (ffmpeg_state.pPacket->stream_index == ffmpeg_state.audioStream) {
            int ret = avcodec_send_packet(ffmpeg_state.aCodecCtx, ffmpeg_state.pPacket);
            if (ret < 0) {
                printf("Error sending packet for decoding.\n");
                exit(-1);
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(ffmpeg_state.aCodecCtx, ffmpeg_state.aFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    // EOF exit loop
                    break;
                } else if (ret < 0) {
                    printf("Error while decoding.\n");
                    exit(-1);
                }

                int dst_samples = av_rescale_rnd(
                                   swr_get_delay(ffmpeg_state.swr_ctx, ffmpeg_state.aFrame->sample_rate) + ffmpeg_state.aFrame->nb_samples,
                                   44100,
                                   ffmpeg_state.aFrame->sample_rate,
                                   AV_ROUND_UP);
                uint8_t *audiobuf = NULL;
                ret = av_samples_alloc(&audiobuf,
                                       NULL,
                                       1,
                                       dst_samples,
                                       AV_SAMPLE_FMT_FLT,
                                       1);
                dst_samples = swr_convert(
                                          ffmpeg_state.swr_ctx,
                                          &audiobuf,
                                          dst_samples,
                                          (const uint8_t**) ffmpeg_state.aFrame->data,
                                          ffmpeg_state.aFrame->nb_samples);
                ret = av_samples_fill_arrays(ffmpeg_state.audioframe->data,
                                             ffmpeg_state.audioframe->linesize,
                                             audiobuf,
                                             1,
                                             dst_samples,
                                             AV_SAMPLE_FMT_FLT,
                                             1);
                saudio_push(reinterpret_cast<float*>(ffmpeg_state.audioframe->data[0]), ffmpeg_state.audioframe->linesize[0]);
            }
        }

        av_packet_unref(ffmpeg_state.pPacket);
    }


}

void cleanup() {
    saudio_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event) {
    switch (event->type) {
        default: break;
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printHelpMenu();
        return EXIT_FAILURE;
    }

    // ffmpeg init
    if (avformat_open_input(&ffmpeg_state.pFormatCtx, argv[1], nullptr, nullptr) < 0) {
        printf("Could not open file %s\n", argv[1]);
        return -1;
    }

    if (avformat_find_stream_info(ffmpeg_state.pFormatCtx, nullptr) < 0) {
        printf("Could not find stream information %s\n", argv[1]);
        return -1;
    }

    av_dump_format(ffmpeg_state.pFormatCtx, 0, argv[1], 0);


    for (int i = 0; i < ffmpeg_state.pFormatCtx->nb_streams; i++) {
        if (ffmpeg_state.pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && ffmpeg_state.videoStream < 0) {
            ffmpeg_state.videoStream = i;
        }
        if (ffmpeg_state.pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && ffmpeg_state.audioStream < 0) {
            ffmpeg_state.audioStream = i;
        }
    }

    if (ffmpeg_state.videoStream == -1) {
        printf("didn't find a video stream\n");
        return -1;
    }

    if (ffmpeg_state.audioStream == -1) {
        printf("Could not find audio stream.\n");
        return -1;
    }

    ffmpeg_state.pCodec = avcodec_find_decoder(ffmpeg_state.pFormatCtx->streams[ffmpeg_state.videoStream]->codecpar->codec_id);
    if (ffmpeg_state.pCodec == nullptr) {
        printf("Unsupported codec! codec not found\n");
        return -1;
    }
    ffmpeg_state.pCodecCtx = avcodec_alloc_context3(ffmpeg_state.pCodec);
    if (avcodec_parameters_to_context(ffmpeg_state.pCodecCtx, ffmpeg_state.pFormatCtx->streams[ffmpeg_state.videoStream]->codecpar) < 0) {
        printf("Could not copy codec context.\n");
        return -1;
    }
    if (avcodec_open2(ffmpeg_state.pCodecCtx, ffmpeg_state.pCodec, nullptr) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }


    ffmpeg_state.aCodec = avcodec_find_decoder(ffmpeg_state.pFormatCtx->streams[ffmpeg_state.audioStream]->codecpar->codec_id);
    if (ffmpeg_state.aCodec == nullptr) {
        printf("Unsupported codec! codec not found\n");
        return -1;
    }
    ffmpeg_state.aCodecCtx = avcodec_alloc_context3(ffmpeg_state.aCodec);
    if (avcodec_parameters_to_context(ffmpeg_state.aCodecCtx, ffmpeg_state.pFormatCtx->streams[ffmpeg_state.audioStream]->codecpar) < 0) {
        printf("Could not copy codec context.\n");
        return -1;
    }
    if (avcodec_open2(ffmpeg_state.aCodecCtx, ffmpeg_state.aCodec, nullptr) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    ffmpeg_state.pFrame     = av_frame_alloc();
    ffmpeg_state.pFrameRGB  = av_frame_alloc();
    ffmpeg_state.aFrame     = av_frame_alloc();
    ffmpeg_state.audioframe = av_frame_alloc(); // resampled frame

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, SCR_WIDTH, SCR_HEIGHT, 32);
    ffmpeg_state.buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(ffmpeg_state.pFrameRGB->data, ffmpeg_state.pFrameRGB->linesize, ffmpeg_state.buffer, AV_PIX_FMT_RGBA, SCR_WIDTH, SCR_HEIGHT, 32);

    ffmpeg_state.sws_ctx = sws_getContext(
    ffmpeg_state.pCodecCtx->width,
    ffmpeg_state.pCodecCtx->height,
    ffmpeg_state.pCodecCtx->pix_fmt,
    SCR_WIDTH,
    SCR_HEIGHT,
    AV_PIX_FMT_RGBA,   // sws_scale destination color scheme
    SWS_BILINEAR,
    nullptr,
    nullptr,
    nullptr
        );

    ffmpeg_state.swr_ctx = swr_alloc_set_opts(
        nullptr,
        AV_CH_LAYOUT_MONO,
        AV_SAMPLE_FMT_FLT,
        44100,
        ffmpeg_state.aCodecCtx->channel_layout,
        ffmpeg_state.aCodecCtx->sample_fmt,
        ffmpeg_state.aCodecCtx->sample_rate,
        0,
        nullptr
        );
    swr_init(ffmpeg_state.swr_ctx);

    ffmpeg_state.pPacket = av_packet_alloc();
    if (ffmpeg_state.pPacket == nullptr) {
        printf("Could not alloc packet,\n");
        return -1;
    }


    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup,
    desc.event_cb = input,
    desc.width  = SCR_WIDTH,
    desc.height = SCR_HEIGHT,
    desc.window_title = "ffmpeg+sokol",
    desc.icon.sokol_default = true,
    desc.logger.func = slog_func;
    sapp_run(desc);


    av_free(ffmpeg_state.buffer);
    av_frame_free(&ffmpeg_state.audioframe);
    av_free(ffmpeg_state.audioframe);
    av_frame_free(&ffmpeg_state.aFrame);
    av_free(ffmpeg_state.aFrame);
    av_frame_free(&ffmpeg_state.pFrameRGB);
    av_free(ffmpeg_state.pFrameRGB);
    av_frame_free(&ffmpeg_state.pFrame);
    av_free(ffmpeg_state.pFrame);
    avcodec_close(ffmpeg_state.pCodecCtx);
    avformat_close_input(&ffmpeg_state.pFormatCtx);
    avcodec_close(ffmpeg_state.aCodecCtx);

    return 0;
}

