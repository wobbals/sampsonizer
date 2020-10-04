//
//  thumb_writer.h
//  sampsonizer
//
//  Created by Charley Robinson on 10/4/20.
//

#ifndef thumb_writer_h
#define thumb_writer_h

#import <libavutil/frame.h>

struct thumb_writer;

int thumb_writer_alloc(struct thumb_writer** writer);
void thumb_writer_free(struct thumb_writer* writer);

// Writes a decoded frame to output file specified by filepath.
// Although any valid path is supported, the writer only writes PNG-encoded
// files.
int thumb_writer_write_frame_to_file(struct thumb_writer* writer,
                                     AVFrame* frame, const char* filepath);

#endif /* thumb_writer_h */
