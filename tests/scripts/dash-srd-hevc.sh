test_begin "dash-srd-hevc"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_1mb.hevc:split_tiles -new $TEMP_DIR/file.mp4" "dash-input-preparation"
do_hash_test $TEMP_DIR/file.mp4 "split-tiles"

do_test "$MP4BOX -dash 1000 -profile live $TEMP_DIR/file.mp4 -out $TEMP_DIR/file.mpd" "basic-dash"

do_hash_test $TEMP_DIR/file.mpd "mpd"
do_hash_test $TEMP_DIR/file_set1_init.mp4 "init"
do_hash_test $TEMP_DIR/file_dash_track1_10.m4s "base_tile"
do_hash_test $TEMP_DIR/file_dash_track2_10.m4s "tt1"
do_hash_test $TEMP_DIR/file_dash_track10_10.m4s "tt9"

myinspect=$TEMP_DIR/inspect_tiles.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect-tiles"

myinspect=$TEMP_DIR/inspect_agg.txt
do_test "$GPAC -i $TEMP_DIR/file.mpd tileagg @ inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect-agg"

test_end
