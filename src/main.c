//
//  main.c
//

#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <assert.h>

#include <libavutil/frame.h>

#include "keyframe_reader.h"
#include "thumb_writer.h"

void usage() {
  printf("usage: sampsonizer -n INTERVAL -i INPUT_PATH -o OUT_TEMPLATE\n");
}

int main(int argc, char **argv)
{
  int c;
  char* input_path = NULL;
  char* output_template = NULL;
  int key_interval = 0;

  static struct option long_options[] =
  {
    {"input", required_argument,        0, 'i'},
    {"output", required_argument,       0, 'o'},
    {"interval", required_argument,     0, 'n'},
    {0, 0, 0, 0}
  };
  /* getopt_long stores the option index here. */
  int option_index = 0;

  while ((c = getopt_long(argc, argv, "i:o:n:",
                          long_options, &option_index)) != -1)
  {
    switch (c)
    {
      case 'i':
        input_path = optarg;
        break;
      case 'o':
        output_template = optarg;
        break;
      case 'n':
        key_interval = atoi(optarg);
        break;
      case '?':
        if (isprint (optopt))
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr,
                  "Unknown option character `\\x%x'.\n",
                  optopt);
        return 1;
      default:
        abort();
    }
  }

  if (!input_path || !output_template || !key_interval) {
    usage();
    return -1;
  }

  // pass process args to keyframe reader
  struct keyframe_reader_config config = { 0 };
  config.src_path = input_path;
  config.keyframe_interval = key_interval;
  struct keyframe_reader* kfr;
  int ret = keyframe_reader_open(&kfr, &config);
  if (ret) {
    fprintf(stderr, "Unable to open source %s: ret=%d\n", input_path, ret);
    return ret;
  }
  // set up thumbnail writer to handle decoded frames
  struct thumb_writer* writer;
  ret = thumb_writer_alloc(&writer);
  if (!writer) {
    fprintf(stderr, "Output file writer configuration failed (ret=%d)\n", ret);
    return ret;
  }
  // output file sequence number increments with each newly written frame
  int seqno = 0;
  char filename[512];
  // transcode all elibigle frames to corresponding output files
  while (!ret) {
    AVFrame* frame = NULL;
    ret = keyframe_reader_get_next_frame(kfr, &frame);
    if (!frame || ret) {
      break;
    }
    printf("read frame pts=%lld\n", frame->pts);

    sprintf(filename, output_template, seqno++);
    ret = thumb_writer_write_frame_to_file(writer, frame, filename);
    av_frame_unref(frame);
  }
  if (ret == AVERROR_EOF) {
    return 0;
  }
  return ret;
}
