

test_comp()
{
test_begin "mp4box-comp"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/base.mp4"
compfile="$TEMP_DIR/comp.mp4"
do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_320x180_128kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new -frag 1000 $mp4file" "add"

do_hash_test $mp4file "add"

do_test "$MP4BOX -comp moof=cmof $mp4file -out $compfile" "comp"
do_hash_test $compfile "comp"

test_end
}

test_comp


