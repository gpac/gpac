test_begin "dash-srd-hevc"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_1mb.hevc:split_tiles -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 $TEMP_DIR/file.mp4 -out $TEMP_DIR/file.mpd" "basic-dash"

do_playback_test "$TEMP_DIR/file.mpd" "play-srd-dash"

test_end
