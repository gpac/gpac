

test_begin "mp4box-wgt"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -package svgb:$MEDIA_DIR/laser/enst_canvas.xml -new $mp4file" "create-package"
do_hash_test $mp4file "create-package"

mp4file="$TEMP_DIR/test2.mp4"
do_test "$MP4BOX -mgt $EXTERNAL_MEDIA_DIR/mpegu/animatedIcon/config.xml -new $mp4file" "create-wgt"
do_hash_test $mp4file "create-wgt"

test_end

