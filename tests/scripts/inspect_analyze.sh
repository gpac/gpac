
test_inspect()
{
 name=$(basename $1)
 name=${name%.*}
 test_begin "inspect-$name"

if [ "$test_skip" = 1 ] ; then
 return
fi

inspect="$TEMP_DIR/inspect.xml"

do_test "$GPAC -i $1 inspect:deep:log=$inspect:analyze" "inspect"
do_hash_test $inspect "inspect"

test_end

}

test_inspect $MEDIA_DIR/auxiliary_files/count_video.cmp
test_inspect $MEDIA_DIR/auxiliary_files/count_english.mp3
test_inspect $MEDIA_DIR/auxiliary_files/enst_video.h264
test_inspect $MEDIA_DIR/auxiliary_files/counter.hvc
test_inspect $MEDIA_DIR/auxiliary_files/video.av1


