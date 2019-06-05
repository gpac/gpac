
hevc_tiles()
{

test_begin "hevctiles-$1"
if [ $test_skip = 1 ] ; then
return
fi

mp4file=$TEMP_DIR/file.mp4
do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_500kb.hevc:$1 -new $mp4file" "split"

do_hash_test "$mp4file" "split"

test_end
}

hevc_tiles "tiles"

