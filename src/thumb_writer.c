//
//  thumb_writer.c
//  sampsonizer
//
//  Created by Charley Robinson on 10/4/20.
//

#include "thumb_writer.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define PNG_PIX_FORMAT AV_PIX_FMT_RGB24
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define DEFAULT_INPUT_FORMAT AV_PIX_FMT_YUV420P

struct thumb_writer {
  AVCodec* encoder;
  AVCodecContext* codec_ctx;
  struct SwsContext* sws_ctx;
  enum AVPixelFormat input_pixel_format;
};

// Creates a new encoder with the specified width and height.
// For most input streams, we assume constant dimensions from one frame to the
// next. This allows for a reasonable caching of those structures.
static int realloc_encoder(struct thumb_writer* writer, int width, int height,
                           enum AVPixelFormat input_pixel_format) {
  // free any preexisting encoder and pixel converter
  if (writer->codec_ctx) {
    avcodec_free_context(&(writer->codec_ctx));
  }
  if (writer->sws_ctx) {
    sws_freeContext(writer->sws_ctx);
    writer->sws_ctx = NULL;
  }
  AVCodec* encoder = writer->encoder;
  AVCodecContext* codec_ctx = avcodec_alloc_context3(encoder);
  codec_ctx->framerate.num = 1;
  codec_ctx->framerate.den = 1;
  codec_ctx->time_base.num = 1;
  codec_ctx->time_base.den = 1;
  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->pix_fmt = encoder->pix_fmts[0];
  int ret = avcodec_open2(codec_ctx, encoder, NULL);
  if (ret) {
    fprintf(stderr, "PNG encoder configuration failed\n");
    return ret;
  }
  struct SwsContext* sws_ctx =
  sws_getContext(width, height, input_pixel_format,
                 width, height, codec_ctx->pix_fmt,
                 0, 0, 0, 0);
  if (!sws_ctx) {
    fprintf(stderr, "pixel converter configuration failed\n");
    return ENODEV;
  }

  writer->input_pixel_format = input_pixel_format;
  writer->codec_ctx = codec_ctx;
  writer->sws_ctx = sws_ctx;
  return 0;
}

int thumb_writer_alloc(struct thumb_writer** writer) {
  int ret;
  AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_PNG);
  if (!encoder) {
    fprintf(stderr, "No PNG encoder detected\n");
    return ENODEV;
  }
  *writer = calloc(1, sizeof(struct thumb_writer));
  (*writer)->encoder = encoder;
  // initialize encoder with guessed width/height/format.
  // this is an opinionated move that assumes it's more likely avcodec/swscale
  // will fail to configure for _any_ width/height/pixfmt than some specific
  // value received in a write call...attempt to catch those errors early.
  ret = realloc_encoder(*writer, DEFAULT_WIDTH, DEFAULT_HEIGHT,
                        DEFAULT_INPUT_FORMAT);
  if (ret) {
    fprintf(stderr, "PNG encoder setup failed (ret=%d)\n", ret);
    thumb_writer_free(*writer);
    return ret;
  }
  return 0;
}

void thumb_writer_free(struct thumb_writer* writer) {
  if (writer->codec_ctx) {
    avcodec_free_context(&(writer->codec_ctx));
  }
  if (writer->sws_ctx) {
    sws_freeContext(writer->sws_ctx);
    writer->sws_ctx = NULL;
  }
  writer->input_pixel_format = AV_PIX_FMT_NONE;
  free(writer);
}

// Convert a frame to the colorspace supported by our encoder.
// In practice, this is RGB, but I tried to make this as generic as possible.
static AVFrame* create_rgb_frame(struct thumb_writer* writer, AVFrame* src) {
  int ret;
  AVFrame* dst = av_frame_alloc();
  if (!dst) {
    return NULL;
  }
  dst->format = writer->codec_ctx->pix_fmt;
  dst->width = src->width;
  dst->height = src->height;
  // detach data buffers from src and dest
  ret = av_frame_get_buffer(dst, 0);
  ret = sws_scale(writer->sws_ctx,
                  (const uint8_t* const*)src->data,
                  src->linesize, 0,
                  src->height,
                  dst->data,
                  dst->linesize);
  if (ret != dst->height) {
    printf("rgb_convert_frame failed\n");
    return NULL;
  }
  return dst;
}

int thumb_writer_write_frame_to_file(struct thumb_writer* writer,
                                     AVFrame* frame, const char* filepath)
{
  int ret = 0;
  int ctx_width = writer->codec_ctx->width;
  int ctx_height = writer->codec_ctx->height;
  enum AVPixelFormat ctx_pix_fmt = writer->input_pixel_format;
  // Check to see if we need to reconfigure the pixel formatter or encoder.
  // This happens when input frames change from whatever previously had been
  // passed.
  if (ctx_width != frame->width || ctx_height != frame->height
      || ctx_pix_fmt != frame->format) {
    ret = realloc_encoder(writer, frame->width, frame->height, frame->format);
  }
  if (ret) {
    fprintf(stderr, "PNG encoder reconfiguration failed (ret=%d)\n", ret);
    return ret;
  }

  // png does not take most likely decoded AVC pixel formats.
  // Convert the frame before encoding.
  AVFrame* thumbnail = create_rgb_frame(writer, frame);
  if (!thumbnail) {
    fprintf(stderr, "Thumbnail colorspace conversion failure\n");
    return ENOTSUP;
  }

  // Encode the frame
  ret = avcodec_send_frame(writer->codec_ctx, thumbnail);
  av_frame_free(&thumbnail);
  if (ret) {
    fprintf(stderr, "png encoder write failed (ret=%d)\n", ret);
    return ret;
  }

  // Read the encoded frame
  AVPacket packet = { 0 };
  ret = avcodec_receive_packet(writer->codec_ctx, &packet);
  if (ret) {
    fprintf(stderr, "png encoder read failed (ret=%d)\n", ret);
    return ret;
  }

  // Dump the frame to file
  FILE* f_out = fopen(filepath, "w");
  if (!f_out) {
    printf("Unable to open file for writing: %s\n", filepath);
    return EIO;
  }
  size_t bytes_out = fwrite(packet.data, 1, packet.size, f_out);
  printf("Wrote %lu bytes to %s\n", bytes_out, filepath);
  ret = fclose(f_out);

  // free the encoded packet
  av_packet_unref(&packet);
  return ret;
}
