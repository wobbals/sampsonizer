//
//  keyframe_reader.c
//  sampsonizer
//
//  Created by Charley Robinson on 10/4/20.
//

#include "keyframe_reader.h"

struct keyframe_reader {
  int64_t key_interval_pts_min;
  int64_t last_packet_pts_decoded;
  AVFormatContext* format_ctx;
  AVCodecContext* codec_ctx;
  int stream_index;
  char is_eof;
};

int keyframe_reader_open(struct keyframe_reader** reader,
                         struct keyframe_reader_config* config)
{
  struct keyframe_reader* result = calloc(1, sizeof(struct keyframe_reader));
  // open the file for parsing
  int ret = avformat_open_input(&(result->format_ctx), config->src_path, NULL,
                                NULL);
  if (ret || !result->format_ctx) {
    keyframe_reader_free(result);
    return ret;
  }

  // parse the file to determine its contents
  ret = avformat_find_stream_info(result->format_ctx, NULL);
  if (ret) {
    keyframe_reader_free(result);
    return ret;
  }

  // select the first video track found in the file.
  char found_stream = 0;
  for (int i = 0; !found_stream && i < result->format_ctx->nb_streams; i++) {
    AVStream* stream = result->format_ctx->streams[i];
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }
    // build a decoder context for this stream.
    AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
      keyframe_reader_free(result);
      return ENODEV;
    }
    result->codec_ctx = avcodec_alloc_context3(decoder);
    if (!result->codec_ctx) {
      keyframe_reader_free(result);
      return ENOMEM;
    }
    ret = avcodec_parameters_to_context(result->codec_ctx, stream->codecpar);
    if (ret) {
      keyframe_reader_free(result);
      return ret;
    }
    result->codec_ctx->framerate = av_guess_frame_rate(result->format_ctx,
                                                       stream, NULL);
    // open the decoder
    ret = avcodec_open2(result->codec_ctx, decoder, NULL);
    if (ret) {
      keyframe_reader_free(result);
      return ret;
    }
    result->stream_index = i;
    result->last_packet_pts_decoded = -1;
    // store output frame interval in the timebase of the parsed stream
    result->key_interval_pts_min =
    (config->keyframe_interval * stream->time_base.den) / stream->time_base.num;
    found_stream = 1;
  }

  if (!found_stream) {
    keyframe_reader_free(result);
    return EINVAL;
  }

  *reader = result;
  return 0;
}

void keyframe_reader_free(struct keyframe_reader* reader)
{
  if (!reader) {
    return;
  }
  if (reader->format_ctx) {
    avformat_free_context(reader->format_ctx);
    reader->format_ctx = NULL;
  }
  free(reader);
}

// Checks a packet for decoding eligibility, packets must:
// 1) Belong to the stream we're interested in parsing
// 2) Be marked as a keyframe.
// 3) Have a pts equal or greater to the last emitted pts + minimum pts interval
static char packet_is_decode_eligible(struct keyframe_reader* reader,
                                      AVPacket* packet)
{
  int64_t min_eligible_pts =
  reader->last_packet_pts_decoded + reader->key_interval_pts_min;

  return (packet->flags & AV_PKT_FLAG_KEY)
  && packet->stream_index == reader->stream_index
  && (reader->last_packet_pts_decoded < 0 || packet->pts >= min_eligible_pts);
}

static int keyframe_reader_read_next(struct keyframe_reader* reader)
{
  AVPacket packet = { 0 };
  int ret = av_read_frame(reader->format_ctx, &packet);
  // discard packets until we encounter another keyframe
  while (!ret && !packet_is_decode_eligible(reader, &packet)) {
    av_packet_unref(&packet);
    ret = av_read_frame(reader->format_ctx, &packet);
  }
  if (ret == AVERROR_EOF) {
    // flush the decoder
    printf("send null pkt\n");
    reader->is_eof = 1;
    ret = 0; // function expected to handle eof; don't surface to caller.
    ret = avcodec_send_packet(reader->codec_ctx, NULL);
  } else if (!ret) {
    printf("send pkt %lld\n", packet.pts);
    ret = avcodec_send_packet(reader->codec_ctx, &packet);
    reader->last_packet_pts_decoded = packet.pts;
  }
  av_packet_unref(&packet);
  return ret;
}

int keyframe_reader_get_next_frame(struct keyframe_reader* reader,
                                   AVFrame** frame)
{
  int ret = 0;
  if (!reader->is_eof) {
    ret = keyframe_reader_read_next(reader);
  }
  if (ret) {
    printf("keyframe_reader_get_next_frame: error on read (ret=%d)\n", ret);
    return ret;
  }

  *frame = av_frame_alloc();
  ret = avcodec_receive_frame(reader->codec_ctx, *frame);
  if (ret == AVERROR(EAGAIN)) {
    printf("EAGAIN\n");
    // need more data fed to decoder before we can emit new frames.
    // I'm not certain why this would be the case if decoder only receives
    // iframes, but here we are.
    return keyframe_reader_get_next_frame(reader, frame);
  }
  return ret;
}
