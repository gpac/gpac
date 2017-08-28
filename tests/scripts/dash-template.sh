#!/bin/sh

base_test()
{

test_begin $1

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

#test bandwidth, time and init
do_test "$MP4BOX -dash 1000 -out $TEMP_DIR/file.mpd $TEMP_DIR/file.mp4$2" "$1-dash"

do_hash_test $TEMP_DIR/file.mpd "$1-hash-mpd"

do_hash_test $TEMP_DIR/$3.mp4 "$1-hash-init"

do_playback_test "$TEMP_DIR/file.mpd" "$1-play"

test_end

}

base_test "dash-template-bandwidth-time" ":bandwidth=600000 -profile live -segment-name test-\$Bandwidth\$-\$Time%05d\$\$Init=is\$" "test-600000-is"

base_test "dash-template-repid-number" ":id=myrep -profile live -segment-name test-\$RepresentationID\$-\$Number%d\$" "test-myrep-"

base_test "dash-template-baseurl-global-path" ":id=myrep -base-url some_dir/ -profile live -segment-name \$Path=some_dir/\$test-\$RepresentationID\$-\$Number%d\$" "some_dir/test-myrep-"

base_test "dash-template-baseurl-rep-path" ":id=myrep:baseURL=some_dir/ -profile live -segment-name \$Path=some_dir/\$test-\$RepresentationID\$-\$Number%d\$" "some_dir/test-myrep-"
