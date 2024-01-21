#include <stdio.h>
#include <chrono>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
};
#include <SDL.h>
#include <SDL_thread.h>

void printHelpMenu()
{
    printf("Invalid arguments.\n\n");
    printf("Usage: ./tutorial02 <filename> <max-frames-to-decode>\n\n");
    printf("e.g: ./tutorial02 /home/rambodrahmani/Videos/Labrinth-Jealous.mp4 200\n");
}

int main(int argc, char * argv[])
{
    if ( !(argc > 2) ) {
        printHelpMenu();
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        printf("Could not initialize SDL - %s\n.", SDL_GetError());
        return -1;
    }

    AVFormatContext* pFormatCtx = nullptr;

    if (avformat_open_input(&pFormatCtx, argv[1], nullptr, nullptr) < 0) {
        printf("Could not open file %s\n", argv[1]);
        return -1;
    }

    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
        printf("Could not find stream information %s\n", argv[1]);
        return -1;
    }

    av_dump_format(pFormatCtx, 0, argv[1], 0);

    int videoStream = -1;
    int audioStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0) {
            videoStream = i;
        }
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0) {
            audioStream = i;
        }
    }

    if (videoStream == -1) {
        printf("didn't find a video stream\n");
        return -1;
    }

    if (audioStream == -1) {
        printf("Could not find audio stream.\n");
        return -1;
    }

    const AVCodec* pCodec = nullptr;
    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == nullptr) {
        printf("Unsupported codec! codec not found\n");
        return -1;
    }
    AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar) < 0) {
        printf("Could not copy codec context.\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    const AVCodec* aCodec = nullptr;
    aCodec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
    if (aCodec == nullptr) {
        printf("Unsupported codec! codec not found\n");
        return -1;
    }
    AVCodecContext* aCodecCtx = avcodec_alloc_context3(aCodec);
    if (avcodec_parameters_to_context(aCodecCtx, pFormatCtx->streams[audioStream]->codecpar) < 0) {
        printf("Could not copy codec context.\n");
        return -1;
    }
    if (avcodec_open2(aCodecCtx, aCodec, nullptr) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    AVFrame* pFrame = av_frame_alloc();
    AVFrame* aFrame = av_frame_alloc();
    AVFrame* audioframe = av_frame_alloc(); // resampled frame

    SDL_Window* screen = SDL_CreateWindow("SDL Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width/2, pCodecCtx->height/2, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    // SDL_Window* screen = SDL_CreateWindow("SDL Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width/2, pCodecCtx->height/2, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!screen) {
        printf("SDL: could not set video mode - exiting.\n");
        return -1;
    }

    SDL_GL_SetSwapInterval(1);

    SDL_Renderer* renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    SDL_zero(have);
    want.freq = pFormatCtx->streams[audioStream]->codecpar->sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = pFormatCtx->streams[audioStream]->codecpar->channels;
    // want.silence = 0;
    // want.samples = 1024;
    SDL_AudioDeviceID auddev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    SDL_PauseAudioDevice(auddev, 0);
    if (!auddev) {
        printf("SDL: coult not open audio device - exiting\n");
        return -1;
    }

    SwsContext* sws_ctx = sws_getContext(
    pCodecCtx->width,
    pCodecCtx->height,
    pCodecCtx->pix_fmt,
    pCodecCtx->width,
    pCodecCtx->height,
    AV_PIX_FMT_YUV420P,   // sws_scale destination color scheme
    SWS_BILINEAR,
    nullptr,
    nullptr,
    nullptr
        );

    SwrContext* swr_ctx = swr_alloc_set_opts(
        nullptr,
        aCodecCtx->channel_layout,
        AV_SAMPLE_FMT_S16,
        44100,
        aCodecCtx->channel_layout,
        aCodecCtx->sample_fmt,
        aCodecCtx->sample_rate,
        0,
        nullptr
        );
    swr_init(swr_ctx);

    AVPacket* pPacket = av_packet_alloc();
    if (pPacket == nullptr) {
        printf("Could not alloc packet,\n");
        return -1;
    }

    uint8_t * buffer = nullptr;
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 32);
    buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

    SDL_Event event;

    AVFrame* pict = av_frame_alloc();

    av_image_fill_arrays(pict->data, pict->linesize, buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 32);

    int maxFramesToDecode;
    sscanf (argv[2], "%d", &maxFramesToDecode);

    int frameIndex = 0;
    while (av_read_frame(pFormatCtx, pPacket) >= 0) {
        if (pPacket->stream_index == videoStream) {
            int ret = avcodec_send_packet(pCodecCtx, pPacket);
            if (ret < 0) {
                printf("Error sending packet for decoding.\n");
                return -1;
            }


            while (ret >= 0) {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    // EOF exit loop
                    break;
                } else if (ret < 0) {
                    printf("Error while decoding.\n");
                    return -1;
                }

                sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pict->data, pict->linesize);

                if (++frameIndex <= maxFramesToDecode) {
                    typedef std::chrono::high_resolution_clock clock;
                    typedef std::chrono::duration<float, std::milli> duration;

                    clock::time_point start = clock::now();

                    SDL_Rect rect;
                    rect.x = 0;
                    rect.y = 0;
                    rect.w = pCodecCtx->width;
                    rect.h = pCodecCtx->height;

                    printf(
                        "Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
                        av_get_picture_type_char(pFrame->pict_type),
                        pCodecCtx->frame_number,
                        pFrame->pts,
                        pFrame->pkt_dts,
                        pFrame->key_frame,
                        pFrame->coded_picture_number,
                        pFrame->display_picture_number,
                        pCodecCtx->width,
                        pCodecCtx->height
                    );

                    SDL_UpdateYUVTexture(texture, &rect, pict->data[0], pict->linesize[0], pict->data[1], pict->linesize[1], pict->data[2], pict->linesize[2]);

                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_RenderPresent(renderer);

                    clock::time_point end = clock::now();
                    duration elapsed = end - start;
                    // double diffms = std::difftime(end, start) / 1000.0;
                    double diffms = elapsed.count();

                    double fps = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);
                    double sleep_time_ms = 1.0/(double)fps * 1000;

                    if (diffms < sleep_time_ms ) {
                        uint32_t diff = (uint32_t)((sleep_time_ms - diffms));
                        printf("diffms: %f, delay time %d ms.\n", diffms, diff);
                        SDL_Delay(diff);
                    }
                } else {
                    break;
                }
            }

        }

        if (pPacket->stream_index == audioStream) {
            int ret = avcodec_send_packet(aCodecCtx, pPacket);
            if (ret < 0) {
                printf("Error sending packet for decoding.\n");
                return -1;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(aCodecCtx, aFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    // EOF exit loop
                    break;
                } else if (ret < 0) {
                    printf("Error while decoding.\n");
                    return -1;
                }

                int dst_samples = aFrame->channels * av_rescale_rnd(
                                   swr_get_delay(swr_ctx, aFrame->sample_rate) + aFrame->nb_samples,
                                   44100,
                                   aFrame->sample_rate,
                                   AV_ROUND_UP);
                uint8_t *audiobuf = NULL;
                ret = av_samples_alloc(&audiobuf,
                                       NULL,
                                       1,
                                       dst_samples,
                                       AV_SAMPLE_FMT_S16,
                                       1);
                dst_samples = aFrame->channels * swr_convert(
                                          swr_ctx,
                                          &audiobuf,
                                          dst_samples,
                                          (const uint8_t**) aFrame->data,
                                          aFrame->nb_samples);
                ret = av_samples_fill_arrays(audioframe->data,
                                             audioframe->linesize,
                                             audiobuf,
                                             1,
                                             dst_samples,
                                             AV_SAMPLE_FMT_S16,
                                             1);
                SDL_QueueAudio(auddev, audioframe->data[0], audioframe->linesize[0]);
            }
        }

        av_packet_unref(pPacket);

        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT: {
                SDL_Quit();
                exit(0);
            }
            break;

            default: {
                // nothing to do
            }
            break;
        }
    }

    av_frame_free(&pict);
    av_free(pict);
    av_free(buffer);

    av_frame_free(&audioframe);
    av_free(audioframe);
    av_frame_free(&aFrame);
    av_free(aFrame);
    av_frame_free(&pFrame);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avcodec_close(aCodecCtx);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    SDL_Quit();

    return 0;
}
