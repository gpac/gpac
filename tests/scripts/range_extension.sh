#!/bin/sh

test_begin "avc-range-extension"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420.h264 -new $TEMP_DIR/counter-bifs-10min-320x180-bipbop_420.mp4" "import-avc-420-8bit"
do_playback_test "$TEMP_DIR/counter-bifs-10min-320x180-bipbop_420.mp4" "play-avc-420-8bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422.h264 -new $TEMP_DIR/counter-bifs-10min-320x180-bipbop_422.mp4" "import-avc-422-8bit"
do_playback_test "$TEMP_DIR/counter-bifs-10min-320x180-bipbop_422.mp4" "play-avc-422-8bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444.h264 -new $TEMP_DIR/counter-bifs-10min-320x180-bipbop_444.mp4" "import-avc-444-8bit"
do_playback_test "$TEMP_DIR/counter-bifs-10min-320x180-bipbop_444.mp4" "play-avc-444-8bit"


do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420_10.h264 -new $TEMP_DIR/counter-bifs-10min-320x180-bipbop_420_10.mp4" "import-avc-420-10bit"
do_playback_test "$TEMP_DIR/counter-bifs-10min-320x180-bipbop_420_10.mp4" "play-avc-420-10bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422_10.h264 -new $TEMP_DIR/counter-bifs-10min-320x180-bipbop_422_10.mp4" "import-avc-422-10bit"
do_playback_test "$TEMP_DIR/counter-bifs-10min-320x180-bipbop_422_10.mp4" "play-avc-422-10bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444_10.h264 -new $TEMP_DIR/counter-bifs-10min-320x180-bipbop_444_10.mp4" "import-avc-444-10bit"
do_playback_test "$TEMP_DIR/counter-bifs-10min-320x180-bipbop_444_10.mp4" "play-avc-444-10bit"

test_end

test_begin "hevc-range-extension"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_420.hvc:fmt=HEVC -new $TEMP_DIR/counter-bifs-1min-640x360-bipbop_420.mp4" "import-hevc-420-8bit"
do_playback_test "$TEMP_DIR/counter-bifs-1min-640x360-bipbop_420.mp4" "play-hevc-420-8bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_422.hvc:fmt=HEVC -new $TEMP_DIR/counter-bifs-1min-640x360-bipbop_422.mp4" "import-hevc-422-8bit"
do_playback_test "$TEMP_DIR/counter-bifs-1min-640x360-bipbop_422.mp4" "play-hevc-422-8bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_444.hvc:fmt=HEVC -new $TEMP_DIR/counter-bifs-1min-640x360-bipbop_444.mp4" "import-hevc-444-8bit"
do_playback_test "$TEMP_DIR/counter-bifs-1min-640x360-bipbop_444.mp4" "play-hevc-444-8bit"


do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_420_10.hvc:fmt=HEVC -new $TEMP_DIR/counter-bifs-1min-640x360-bipbop_420_10.mp4" "import-hevc-420-10bit"
do_playback_test "$TEMP_DIR/counter-bifs-1min-640x360-bipbop_420_10.mp4" "play-hevc-420-10bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_422_10.hvc:fmt=HEVC -new $TEMP_DIR/counter-bifs-1min-640x360-bipbop_422_10.mp4" "import-hevc-422-10bit"
do_playback_test "$TEMP_DIR/counter-bifs-1min-640x360-bipbop_422_10.mp4" "play-hevc-422-10bit"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/rext/counter-bifs-1min-640x360-bipbop_444_10.hvc:fmt=HEVC -new $TEMP_DIR/counter-bifs-1min-640x360-bipbop_444_10.mp4" "import-hevc-444-10bit"
do_playback_test "$TEMP_DIR/counter-bifs-1min-640x360-bipbop_444_10.mp4" "play-hevc-444-10bit"

test_end
