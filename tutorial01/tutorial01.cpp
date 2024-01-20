#include <stdio.h>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
};

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

void printHelpMenu()
{
    printf("Invalid arguments.\n\n");
    printf("Usage: ./tutorial01 <filename> <max-frames-to-decode>\n\n");
    printf("e.g: ./tutorial01 /home/rambodrahmani/Videos/Labrinth-Jealous.mp4 200\n");
}

void saveFrame(AVFrame * avFrame, int width, int height, int frameIndex)
{
    char szFilename[32];
    sprintf(szFilename, "frame%d.png", frameIndex);
    stbi_write_jpg(szFilename, width, height, 3, avFrame->data[0], 75);
}

int main(int argc, char * argv[])
{
    if ( !(argc > 2) ) {
        printHelpMenu();
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

    int i;

    AVCodecContext* pCodecCtxOrig = nullptr; // never use it
    AVCodecContext* pCodecCtx = nullptr;

    int videoStream = -1;
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

    const AVCodec* pCodec = nullptr;
    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == nullptr) {
        printf("Unsupported codec! codec not found\n");
        return -1;
    }

    pCodecCtxOrig = avcodec_alloc_context3(pCodec); // [7]
    if (avcodec_parameters_to_context(pCodecCtxOrig, pFormatCtx->streams[videoStream]->codecpar) < 0) {
        printf("Could not copy codec context.\n");
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

    AVFrame* pFrame = nullptr;
    pFrame = av_frame_alloc();
    if (pFrame == nullptr) {
        printf("Could not allocate frame.\n");
        return -1;
    }

    AVFrame * pFrameRGB = nullptr;
    pFrameRGB = av_frame_alloc();
    if (pFrameRGB == nullptr) {
        printf("Could not allocate frame.\n");
        return -1;
    }

    uint8_t* buffer = nullptr;
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);
    buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);

    SwsContext* sws_ctx = nullptr;

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
    AV_PIX_FMT_RGB24,   // sws_scale destination color scheme
    SWS_BILINEAR,
    nullptr,
    nullptr,
    nullptr
        );

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

                sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

                if (++i <= maxFramesToDecode) {
                    saveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);

                    printf(
                        "Frame %c (%d) pts %d dts %d key_frame %d "
            "[coded_picture_number %d, display_picture_number %d,"
            " %dx%d]\n",
                        av_get_picture_type_char(pFrame->pict_type),
                        pCodecCtx->frame_number,
                        pFrameRGB->pts,
                        pFrameRGB->pkt_dts,
                        pFrameRGB->key_frame,
                        pFrameRGB->coded_picture_number,
                        pFrameRGB->display_picture_number,
                        pCodecCtx->width,
                        pCodecCtx->height
                    );
                } else {
                    break;
                }
            }

            av_packet_unref(pPacket);
        }
    }

    av_free(buffer);
    av_frame_free(&pFrameRGB);
    av_free(pFrameRGB);

    av_frame_free(&pFrame);
    av_free(pFrame);

    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    avformat_close_input(&pFormatCtx);

    return 0;
}
