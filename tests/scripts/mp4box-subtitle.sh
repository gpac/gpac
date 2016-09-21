#!/bin/sh

my_test()
{
	do_test "$1 $2" $3
	do_hash_test $2 $3
}

test_begin "mp4box-subtitle"

 if [ $test_skip  = 1 ] ; then
  return
 fi

my_test "$MP4BOX -ttxt $MEDIA_DIR/auxiliary_files/subtitle.srt -out" "$TEMP_DIR/subtitle-srt.ttxt" "srt-to-ttxt-out"

my_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt -new" "$TEMP_DIR/subtitle-srt-tx3g.mp4" "srt-add"

my_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt:fmt=VTT -new" "$TEMP_DIR/subtitle-srt-wvtt.mp4" "srt-vtt-add"

my_test "$MP4BOX -add $TEMP_DIR/subtitle-srt.ttxt -new" "$TEMP_DIR/subtitle-ttxt-tx3g.mp4" "tx3g-add"

my_test "$MP4BOX -add $MEDIA_DIR/webvtt/spec-example-basic.vtt -new" "$TEMP_DIR/subtitle-vtt-wvtt.mp4" "vtt-add"

my_test "$MP4BOX -add $MEDIA_DIR/ttml/ebu-ttd_sample.ttml -new" "$TEMP_DIR/subtitle-ttml-stpp.mp4" "ttml-add"

my_test "$MP4BOX -ttxt 1 $TEMP_DIR/subtitle-srt-tx3g.mp4 -out" "$TEMP_DIR/subtitle-tx3g.ttxt" "tx3g-dump-track-out"

my_test "$MP4BOX -srt 1 $TEMP_DIR/subtitle-ttxt-tx3g.mp4 -out" "$TEMP_DIR/subtitle-tx3g.srt" "srt-dump-track-out"

my_test "$MP4BOX -srt $TEMP_DIR/subtitle-srt.ttxt -out" "$TEMP_DIR/subtitle-ttxt.srt" "ttxt-to-srt-out"

my_test "$MP4BOX -svg $MEDIA_DIR/auxiliary_files/subtitle.srt -out" "$TEMP_DIR/subtitle-srt.svg" "srt-to-svg-out"

my_test "$MP4BOX -svg $TEMP_DIR/subtitle-srt.ttxt -out" "$TEMP_DIR/subtitle-ttxt.svg" "ttxt-to-svg-out"

my_test "$MP4BOX -raw 1:output=$TEMP_DIR/subtitle-out.vtt" "$TEMP_DIR/subtitle-srt-wvtt.mp4" "vtt-raw-output"

my_test "$MP4BOX -diso $MEDIA_DIR/auxiliary_files/subs.ismt -out" "$TEMP_DIR/out_info.xml" "fragmented-ttml"

test_end
