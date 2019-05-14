

test_mode()
{
test_begin "mp4box-$1"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file -$1" "create-$1"
do_hash_test $mp4file "create-$1"

hintfile="$TEMP_DIR/testh.mp4"
do_test "$MP4BOX -hint -ocr $mp4file -out $hintfile" "hint-$1"
do_hash_test $hintfile "hint-$1"

test_end
}

test_mode "3gp"
test_mode "isma"
test_mode "ismax"
test_mode "ipod"
test_mode "psp"
