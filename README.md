_I do bite my thumbnail, sir._

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

`sampsonizer -n 10 input.mp4 out%04d.png`

# Notes & errata for reviewer

## alternative approximate solution

Depending on the interpretation of the problem statement, much of this problem
could be implemented with a `one-liner` gstreamer pipeline, for example:

```
# -n 10 ~/Downloads/BigBuckBunny.mp4 frame%d.png
gst-launch-1.0 filesrc location=~/Downloads/BigBuckBunny.mp4 \
! qtdemux ! h264parse ! identity drop-buffer-flags=8192 \
! avdec_h264 ! videorate drop-only=true ! video/x-raw,framerate=1/10 \
! videoconvert ! pngenc ! multifilesink location="frame%d.png"

```

The only thing missing from this approach (that I noticed anyway) is that
in cases where the output rate is significantly lower than the keyframe rate
of the source bitstream, keyframes that do not need decoding are nonetheless
decoded before being dropped by the videorate element. I had considered
extending `identity` with a rate parameter, or even writing a fresh gstreamer
plugin to cover this quirk, but the whole exercise began to feel a bit detached
from the spirit of the original problem statement.

There's plenty of great discussion to be had about the trade-offs between
modular/reusable and bespoke solutions to problems like these.
I chose to interpret this prompt as a _programming_ sample, rather than a
_solution_ demonstration, and would welcome the bigger picture discussion as
follow-up to the reviewer.

Thus, I present sampsonizer, the sparse bitstream sampler.

## Style

I find ffmpegs C libraries pleasant to work with, but force me to adopt a code
style that I don't typically use. As such, there may be some lines of code that
seem a bit inconsistent.

I also tend to forget the semantics of libavformat and libavcodec nearly every
time I pick the libraries up to make tools like these. I've done my best to
annotate steps in-line and add rationale to decisions that felt interesting at
the time of writing.

## Error handling and logging

Most of the effort in maintaining tools like these is not in the "happy path",
but rather the graceful handling of errors and introducing tools to aid
debuggers while not drowning stdout/stderr in messages that the cilent may not
find so helpful. This example introduces no such facility, but I find this to
often be an interesting topic for further consideration.
