#!/bin/sh

#todo - rewrite this test completely ...

test_begin "hls"

m3u8file=$EXTERNAL_MEDIA_DIR/hls/index.m3u8
mpdfile=$EXTERNAL_MEDIA_DIR/hls/file.mpd

do_test "$MP4BOX -mpd $m3u8file -out $mpdfile" "convert"

rm -rf $mpdfile

test_end
