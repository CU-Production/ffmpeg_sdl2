#include <stdio.h>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
};
#include <SDL.h>
#include <SDL_thread.h>

void printHelpMenu()
{
    printf("Invalid arguments.\n\n");
    printf("Usage: ./tutorial03 <filename> <max-frames-to-decode>\n\n");
    printf("e.g: ./tutorial03 /home/rambodrahmani/Videos/Labrinth-Jealous.mp4 200\n");
}

int main(int argc, char * argv[])
{
    AVFormatContext *pFormatCtx = nullptr;
    int             i, videoStream, audioStream;
    AVCodecContext  *pCodecCtx = nullptr;
    const AVCodec   *pCodec = nullptr;
    AVFrame         *pFrame = nullptr;
    AVPacket        packet;
    int             frameFinished;
    struct SwsContext *sws_ctx = nullptr;


    if ( !(argc > 2) ) {
        printHelpMenu();
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        printf("Could not initialize SDL - %s\n.", SDL_GetError());
        return -1;
    }

    if (avformat_open_input(&pFormatCtx, argv[1], nullptr, nullptr) < 0) {
        printf("Could not open file %s\n", argv[1]);
        return -1;
    }

    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
        printf("Could not find stream information %s\n", argv[1]);
        return -1;
    }

    av_dump_format(pFormatCtx, 0, argv[1], 0);

    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1) {
        printf("didn't find a video stream\n");
        return -1;
    }

    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == nullptr) {
        printf("Unsupported codec! codec not found\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar) < 0) {
        printf("Could not copy codec context.\n");
        return -1;
    }

    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    if (pFrame == nullptr) {
        printf("Could not allocate frame.\n");
        return -1;
    }

    SDL_Window* screen = SDL_CreateWindow("SDL Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width/2, pCodecCtx->height/2, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    // SDL_Window* screen = SDL_CreateWindow("SDL Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width/2, pCodecCtx->height/2, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!screen) {
        printf("SDL: could not set video mode - exiting.\n");
        return -1;
    }

    SDL_GL_SetSwapInterval(1);

    SDL_Renderer* renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

    AVPacket* pPacket = av_packet_alloc();
    if (pPacket == nullptr) {
        printf("Could not alloc packet,\n");
        return -1;
    }

    sws_ctx = sws_getContext(
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

    uint8_t * buffer = nullptr;
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 32);
    buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

    SDL_Event event;

    AVFrame* pict = av_frame_alloc();

    av_image_fill_arrays(pict->data, pict->linesize, buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 32);

    int maxFramesToDecode;
    sscanf (argv[2], "%d", &maxFramesToDecode);

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

                if (++i <= maxFramesToDecode) {
                    double fps = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);
                    double sleep_time = 1.0/(double)fps;
                    SDL_Delay((1000 * sleep_time) - 10);

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
                } else {
                    break;
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
    }

    av_frame_free(&pict);
    av_free(pict);
    av_free(buffer);

    av_frame_free(&pFrame);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    SDL_Quit();

    return 0;
}
