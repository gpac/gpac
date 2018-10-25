#!/bin/sh

test_begin "hls-gen-files"

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash 1000 -profile dashavc264:live $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -segment-name test-\$RepresentationID\$-\$Number%d\$ -out $TEMP_DIR/file.m3u8" "hls-gen"

do_hash_test "$TEMP_DIR/file.m3u8" "hls-master"
do_hash_test "$TEMP_DIR/file_1.m3u8" "hls-pl1"
do_hash_test "$TEMP_DIR/file_2.m3u8" "hls-pl2"

do_hash_test "$TEMP_DIR/test-1-.mp4" "hls-init1"
do_hash_test "$TEMP_DIR/test-2-.mp4" "hls-init2"

do_hash_test "$TEMP_DIR/test-1-10.m4s" "hls-seg1_10"
do_hash_test "$TEMP_DIR/test-2-10.m4s" "hls-seg2_10"

test_end


test_begin "hls-gen-byteranges"

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash 1000 -profile dashavc264:onDemand $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.m3u8" "hls-gen"

do_hash_test "$TEMP_DIR/file.m3u8" "hls-master"
do_hash_test "$TEMP_DIR/file_1.m3u8" "hls-pl1"
do_hash_test "$TEMP_DIR/file_2.m3u8" "hls-pl2"

test_end

