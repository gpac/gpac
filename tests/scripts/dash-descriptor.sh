test_begin "dash-descriptor"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=1 -new $TEMP_DIR/file.mp4" "dash-input-preparation"

do_test "$MP4BOX -dash 1000 -rap $TEMP_DIR/file.mp4:desc_rep=<foo>bar</foo> -out $TEMP_DIR/file_rep.mpd" "desc_rep"

do_hash_test $TEMP_DIR/file_rep.mpd "desc_rep"

do_test "$MP4BOX -dash 1000 -rap $TEMP_DIR/file.mp4:desc_as=<foo>bar</foo> $TEMP_DIR/file.mp4:desc_as=<foo>bar2</foo> -out $TEMP_DIR/file_as.mpd" "desc_as"

do_hash_test $TEMP_DIR/file_as.mpd "desc_as"

do_test "$MP4BOX -dash 1000 -rap $TEMP_DIR/file.mp4:desc_as=<foo>bar</foo> $TEMP_DIR/file.mp4:desc_as=<foo>bar</foo> -out $TEMP_DIR/file_as_same.mpd" "desc_as_same"

do_hash_test $TEMP_DIR/file_as_same.mpd "desc_as_same"


do_test "$MP4BOX -dash 1000 -rap $TEMP_DIR/file.mp4:desc_as=<foo>bar</foo> $TEMP_DIR/file.mp4:desc_as=<foo>bar</foo> -out $TEMP_DIR/file_as_c.mpd" "desc_as_c"

do_hash_test $TEMP_DIR/file_as_c.mpd "desc_as_c"

do_test "$MP4BOX -dash 1000 -rap $TEMP_DIR/file.mp4:desc_p=<foo>bar</foo> -out $TEMP_DIR/file_p.mpd" "desc_p"

do_hash_test $TEMP_DIR/file_p.mpd "desc_p"

test_end
