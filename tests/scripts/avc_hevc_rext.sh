#!/bin/sh

single_playback_test "$MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444_10_avc_gop_24.mp4" "avc-444-10bit"
single_playback_test "$MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422_10_avc_gop_24.mp4" "avc-422-10bit"
single_playback_test "$MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420_10_avc_gop_24.mp4" "avc-420-10bit"

single_playback_test "$MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_444_avc_gop_24.mp4" "avc-444-8bit"
single_playback_test "$MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_422_avc_gop_24.mp4" "avc-422-8bit"
single_playback_test "$MEDIA_DIR/rext/counter-bifs-10min-320x180-bipbop_420_avc_gop_24.mp4" "avc-420-8bit"

single_playback_test "$MEDIA_DIR/rext/TSCTX_10bit_RExt_SHARP_1.bin.mp4" "hevc-444-10bit"
single_playback_test "$MEDIA_DIR/rext/Main_422_10_B_RExt_Sony_1.bin.mp4" "hevc-422-10bit"
single_playback_test "$MEDIA_DIR/rext/BasketballPass_416x240_50_qp37.bin.mp4" "hevc-420-10bit"

single_playback_test "$MEDIA_DIR/rext/CCP_8bit_RExt_QCOM.bin.mp4" "hevc-444-8bit"
single_playback_test "$MEDIA_DIR/rext/Main_422_B_RExt_Sony_1.bin.mp4" "hevc-422-8bit"
single_playback_test "$MEDIA_DIR/rext/AMP_A_Samsung_6.bit.bin.mp4" "hevc-420-8bit"



