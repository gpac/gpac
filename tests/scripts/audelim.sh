
test_au_delim()
{
test_begin "$1-au-delim"
if [ "$test_skip" = 1 ] ; then
 return
fi

mp4file="$TEMP_DIR/test.mp4"
rawfile="$TEMP_DIR/test.$1"

do_test "$MP4BOX -add $2 -new $mp4file" "create-mp4"
do_hash_test $mp4file "create-mp4"

do_test "$MP4BOX -raw 1 $mp4file -out $rawfile" "dump-mp4"

mp4file="$TEMP_DIR/test-aud.mp4"
do_test "$MP4BOX -add $rawfile:au_delim -new $mp4file" "audelim"
do_hash_test $mp4file "audelim"

mp4file="$TEMP_DIR/test-xpsib.mp4"
do_test "$MP4BOX -add $rawfile:xps_inband -new $mp4file" "xpsinband"
do_hash_test $mp4file "xpsinband"

mp4file="$TEMP_DIR/test-xpsib-aud.mp4"
do_test "$MP4BOX -add $rawfile:xps_inband:au_delim -new $mp4file" "xpsinband_audelim"
do_hash_test $mp4file "xpsinband_audelim"

test_end

}

test_au_delim "avc" "$MEDIA_DIR/auxiliary_files/enst_video.h264"

test_au_delim "hevc" "$MEDIA_DIR/auxiliary_files/counter.hvc"

