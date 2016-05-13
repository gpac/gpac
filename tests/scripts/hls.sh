#!/bin/sh

test_begin "hls"

m3u8file=$EXTERNAL_MEDIA_DIR/hls/index.m3u8
mpdfile=$EXTERNAL_MEDIA_DIR/hls/file.mpd

do_test "$MP4BOX -mpd $m3u8file -out $mpdfile" "convert-hls-dash"
do_playback_test "$m3u8file" "basic-hls-playback"
do_playback_test "$mpdfile" "converted-dash-playback"

rm -rf $mpdfile

test_end
