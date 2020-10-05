//
//  keyframe_reader.h
//  sampsonizer
//
//  Created by Charley Robinson on 10/4/20.
//

#ifndef keyframe_reader_h
#define keyframe_reader_h

#include <libavformat/avformat.h>
#include <libavutil/frame.h>

// Simple wrapper around an input file. Emits only frames of the first video
// stream parsed in the input container, marked as keyframes, at the specified
// keyframe_interal passed in config.
struct keyframe_reader;

struct keyframe_reader_config {
  int keyframe_interval;
  const char* src_path;
};

/*
 @return In addition to retcodes offered in errors from libavformat and
 libavcodec, this function may return:
 EINVAL - No appropriate stream could be found in the input file
 ENODEV - Decoder could not be configured for this input
 ENOMEM - Allocation of the keyframe reader or its children failed.
 */
int keyframe_reader_open(struct keyframe_reader** reader,
                         struct keyframe_reader_config* config);
void keyframe_reader_free(struct keyframe_reader* reader);

/*
 Fetches the next eligible frame for the instance.
 @return 0 on success, and any error that can be returned by
 av_read_frame, avcodec_receive_frame, or avcodec_send_packet.
 */
int keyframe_reader_get_next_frame(struct keyframe_reader* reader,
                                   AVFrame** frame);

#endif /* keyframe_reader_h */
