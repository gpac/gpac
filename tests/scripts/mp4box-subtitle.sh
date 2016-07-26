#!/bin/sh

test_begin "mp4box-subtitle"

do_test "$MP4BOX -ttxt $MEDIA_DIR/auxiliary_files/subtitle.srt -out $TEMP_DIR/subtitle-srt.ttxt" "srt-to-ttxt-out"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt -new $TEMP_DIR/subtitle-srt-tx3g.mp4" "srt-add"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt:fmt=VTT -new $TEMP_DIR/subtitle-srt-wvtt.mp4" "srt-vtt-add"
do_test "$MP4BOX -add $TEMP_DIR/subtitle-srt.ttxt -new $TEMP_DIR/subtitle-ttxt-tx3g.mp4" "tx3g-add"
do_test "$MP4BOX -add $MEDIA_DIR/webvtt/spec-example-basic.vtt -new $TEMP_DIR/subtitle-vtt-wvtt.mp4" "vtt-add"
do_test "$MP4BOX -add $MEDIA_DIR/ttml/ebu-ttd_sample.ttml -new $TEMP_DIR/subtitle-ttml-stpp.mp4" "ttml-add"

#do_test "$MP4BOX -ttxt $TEMP_DIR/subtitle-srt-tx3g.mp4" "tx3g-dump-file"
#do_test "$MP4BOX -ttxt $TEMP_DIR/subtitle-srt-tx3g.mp4 -out $TEMP_DIR/subtitle-tx3g.ttxt" "tx3g-dump-file-out"
#do_test "$MP4BOX -ttxt 1 $TEMP_DIR/subtitle-srt-tx3g.mp4" "tx3g-dump-track"
do_test "$MP4BOX -ttxt 1 $TEMP_DIR/subtitle-srt-tx3g.mp4 -out $TEMP_DIR/subtitle-tx3g.ttxt" "tx3g-dump-track-out"
#do_test "$MP4BOX -ttxt 1:output=$TEMP_DIR/subtitle-tx3g-1.ttxt $TEMP_DIR/subtitle-srt-tx3g.mp4" "tx3g-dump-track-output"

#do_test "$MP4BOX -srt $TEMP_DIR/subtitle-ttxt-tx3g.mp4" "srt-dump-file"
#do_test "$MP4BOX -srt $TEMP_DIR/subtitle-ttxt-tx3g.mp4 -out $TEMP_DIR/subtitle-tx3g.srt" "srt-dump-file-out"
#do_test "$MP4BOX -srt 1 $TEMP_DIR/subtitle-ttxt-tx3g.mp4" "srt-dump-track"
do_test "$MP4BOX -srt 1 $TEMP_DIR/subtitle-ttxt-tx3g.mp4 -out $TEMP_DIR/subtitle-tx3g.srt" "srt-dump-track-out"
#do_test "$MP4BOX -srt 1:output=$TEMP_DIR/subtitle-tx3g-1.srt $TEMP_DIR/subtitle-ttxt-tx3g.mp4" "srt-dump-track-output"

#do_test "$MP4BOX -srt $TEMP_DIR/subtitle-tx3g.ttxt" "ttxt-to-srt"
do_test "$MP4BOX -srt $TEMP_DIR/subtitle-srt.ttxt -out $TEMP_DIR/subtitle-ttxt.srt" "ttxt-to-srt-out"
#do_test "$MP4BOX -ttxt $TEMP_DIR/subtitle.srt" "srt-to-ttxt"
#do_test "$MP4BOX -ttxt $TEMP_DIR/subtitle.srt -out $TEMP_DIR/subtitle-srt.ttxt" "srt-to-ttxt-out"

#do_test "$MP4BOX -svg $MEDIA_DIR/subtitle.srt" "srt-to-svg"
do_test "$MP4BOX -svg $MEDIA_DIR/auxiliary_files/subtitle.srt -out $TEMP_DIR/subtitle-srt.svg" "srt-to-svg-out"
#do_test "$MP4BOX -svg $MEDIA_DIR/subtitle.ttxt" "ttxt-to-svg"
do_test "$MP4BOX -svg $TEMP_DIR/subtitle-srt.ttxt -out $TEMP_DIR/subtitle-ttxt.svg" "ttxt-to-svg-out"

#do_test "$MP4BOX -vtt *"
#do_test "$MP4BOX -ttml *"

#do_test "$MP4BOX -raw 1 $TEMP_DIR/subtitle-srt-wvtt.mp4" "vtt-raw"
do_test "$MP4BOX -raw 1:output=$TEMP_DIR/subtitle-out.vtt $TEMP_DIR/subtitle-srt-wvtt.mp4" "vtt-raw-output"
#do_test "$MP4BOX -raw 1 $TEMP_DIR/subtitle-ttml-stpp.mp4" "ttml-raw"
#do_test "$MP4BOX -raw 1:output=$TEMP_DIR/subtitle-out.ttml $TEMP_DIR/subtitle-ttml-stpp.mp4" "ttml-raw-output"

do_test "$MP4BOX -diso $MEDIA_DIR/auxiliary_files/subs.ismt -out $TEMP_DIR/out_info.xml" "fragmented-ttml"

test_end
