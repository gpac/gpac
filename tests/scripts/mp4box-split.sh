

test_split()
{
test_begin "mp4box-split-$1"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/base.mp4"
do_test "$MP4BOX -add $2:dur=10 -new $mp4file" "add"
do_hash_test $mp4file "add"

do_test "$MP4BOX -split 1 $mp4file" "split-1s"
do_hash_test $TEMP_DIR/base_001.mp4 "split-1"
do_hash_test $TEMP_DIR/base_005.mp4 "split-5"
do_hash_test $TEMP_DIR/base_009.mp4 "split-9"

do_test "$MP4BOX -splitx 4:8.5 $mp4file" "splitx"
do_hash_test $TEMP_DIR/base_001.mp4 "splitx"

do_test "$MP4BOX -splitz 4:7.5 $mp4file" "splitz"
do_hash_test $TEMP_DIR/base_001.mp4 "splitz"


mv $mp4file $TEMP_DIR/base_s.mp4
mp4file="$TEMP_DIR/base_s.mp4"

do_test "$MP4BOX -splits 100 $mp4file" "split-100kb"
do_hash_test $TEMP_DIR/base_001.mp4 "splits-1"
do_hash_test $TEMP_DIR/base_002.mp4 "splits-2"


test_end
}

test_split "avc" $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_320x180_128kbps.264

test_split "aac" $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac


