_I do bite my thumb[nail], sir._

## Requirements

* cmake (3.5)
* ffmpeg (used branch `release/4.3` in test)

```
mkdir build && cd build && cmake .. && make
```

## Usage

`./sampsonizer -n INTERVAL -i INFILE -o OUTTEMPLATE`

* `-n INTERVAL` -- the interval, in seconds, between output thumbs
* `-i INFILE` -- the input file the path to an mp4 with an H.264 video track
* `-o OUTTEMPLATE` -- a sprintf-style template string that resolves output thumbs

### Example:

`sampsonizer -n 10 -i /tmp/BigBuckBunny.mp4 /tmp/frame%04d.png`

# Notes, errata, and editorial for reviewers

## alternative approximate solution

Before I dive in to a code solution to this prompt, it's worth stepping back
to look at what's being asked:

Depending on interpretation of the original problem statement, much of this
problem could be implemented in a "one-liner" gstreamer pipeline, for example:

```
# -n 10 ~/Downloads/BigBuckBunny.mp4 frame%d.png
gst-launch-1.0 filesrc location=~/Downloads/BigBuckBunny.mp4 \
! qtdemux ! h264parse ! identity drop-buffer-flags=8192 \
! avdec_h264 ! videorate drop-only=true ! video/x-raw,framerate=1/10 \
! videoconvert ! pngenc ! multifilesink location="frame%d.png"

```

The only thing missing from this approach (that I noticed anyway) is an
inefficiency in cases where the output rate is significantly lower than the
keyframe rate of the source bitstream. Here, keyframes that do not need
decoding are nonetheless decoded before being dropped by the videorate element.
I had considered addressing this quirk as patches or new gstreamer elements,
but the approach began to feel a bit detached from the spirit of the original
problem statement, which calls specifically for a command line tool.

There's plenty of great discussion to be had about the trade-offs between
modular/reusable and bespoke solutions to problems like these.
I chose to interpret this prompt as a request for a _programming_ sample,
rather than a _solution_ demonstration, and would welcome that discussion as
follow-up to the reviewer.

Thus, I present sampsonizer, the sparse bitstream sampler.

## Style

I find ffmpeg's C libraries pleasant to work with, but force me to adopt a code
style that I don't typically use. As such, there may be some lines of code that
seem a bit inconsistent.

I also tend to forget the semantics of libavformat and libavcodec nearly every
time I pick the libraries up to make tools like these. I've done my best to
annotate steps in-line and add rationale to decisions that felt interesting at
the time of writing.

## Error handling and logging

Most of the effort in maintaining tools like these is not so much in writing
code to implement the "happy path", but rather the graceful handling of errors
and tools to make the tool more robust and client lives easier. Here, we fall
short in a few ways that could be interesting to discuss or expand upon:

* Adding tools to redirect/capture/configure logging output so as to prevent
drowning clients in stdout/stderr messages that are only useful to a debugger
of this library.
* Testing against a variety of input files, formats, intervals (both thumbnail
rate and GOP length).
* Expanding to support multiple output formats
* Graceful handling of errors. This tool takes a somewhat brutalist approach to
managing the errors and retcodes that arise from calls in to ffmpeg libraries.
* Complete the various `_free` implementations. These fell short as I ran out of
time to work on this problem. In a situation where the main function exit is an
exited process, this seemed like an acceptable sacrifice.
