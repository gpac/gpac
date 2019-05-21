
test_sar()
{

name=$(basename $1)
name=${name%.*}
test_begin "mp4box-sar-$name"

if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -add $1 -par 1=16:9 -new $mp4file" "create"
do_hash_test $mp4file "create"

#do_test "$MP4BOX -sar 1:1 $mp4file" "resetsar"
#do_hash_test $mp4file "resetsar"

test_end
}

test_sar $MEDIA_DIR/auxiliary_files/enst_video.h264

test_sar $MEDIA_DIR/auxiliary_files/count_video.cmp

test_sar $MEDIA_DIR/auxiliary_files/counter.hvc

