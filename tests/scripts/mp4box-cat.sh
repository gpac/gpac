

test_cat()
{
test_begin "mp4box-cat-$1"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test-addcat.mp4"
do_test "$MP4BOX -add $2 -cat $2 -new $mp4file" "addcat"
do_hash_test $mp4file "addcat"

if [ $3 != 0 ] ; then

mp4file="$TEMP_DIR/base.mp4"
do_test "$MP4BOX -add $2 -new $mp4file" "add"
do_hash_test $mp4file "add"
catfile="$TEMP_DIR/test-add-cat.mp4"
do_test "$MP4BOX -cat $2 $mp4file -out $catfile" "cat"
do_hash_test $catfile "cat"

catfile="$TEMP_DIR/test-catmp4.mp4"
do_test "$MP4BOX -cat $mp4file $mp4file -out $catfile" "catmp4"
do_hash_test $catfile "catmp4"

#generate playlist in current dir since we put relative path in it
plfile="pl.txt"
echo $2 > $plfile
echo $2 >> $plfile

catfile="$TEMP_DIR/test-catpl.mp4"
do_test "$MP4BOX -catpl $plfile -new $catfile" "catpl"
do_hash_test $catfile "catpl"

catfile="$TEMP_DIR/test-catplmp4.mp4"
do_test "$MP4BOX -catpl $plfile $mp4file -out $catfile" "catplmp4"
do_hash_test $catfile "catplmp4"

rm $plfile

fi

test_end
}

test_cat_merge()
{
test_begin "mp4box-catmerge-$1"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/file.mp4"
do_test "$MP4BOX -cat $2 -cat $3 -new $mp4file" "mergecat"
do_hash_test $mp4file "mergecat"

test_end
}


test_cat "avc" $MEDIA_DIR/auxiliary_files/enst_video.h264 1
test_cat "hevc" $MEDIA_DIR/auxiliary_files/counter.hvc 0
test_cat "aac" $MEDIA_DIR/auxiliary_files/enst_audio.aac 0
test_cat "srt" $MEDIA_DIR/auxiliary_files/subtitle.srt 0

test_cat_merge "avc" $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_320x180_128kbps.264 $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_640x360_192kbps.264

test_cat_merge "hevc" $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_untiled_200k.hevc $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_1mb.hevc


test_begin "mp4box-catmulti"
if [ "$test_skip" != 1 ] ; then


mp4file="$TEMP_DIR/file.mp4"
insp="$TEMP_DIR/inspect.txt"
#we cannot hash the result because -cat * will call enum_directory which may behave differently across platforms
do_test "$MP4BOX -cat $EXTERNAL_MEDIA_DIR/counter/@.hevc -new $mp4file" "cat"

myres=`MP4Box -info $mp4file 2>&1 | grep "5250 samples"`

if [ -z "$myres" ] ; then
result="error importing"
fi

fi
test_end

