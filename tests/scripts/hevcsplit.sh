#test hevcsplit filter

hevcsplit_test()
{

test_begin "hevcsplit-$1"
if [ $test_skip = 1 ] ; then
return
fi
do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_$1.hevc hevcsplit:FID=1 -o $TEMP_DIR/dump_\$CropOrigin\$.hvc:SID=1#CropOrigin=*" "split"

for i in $TEMP_DIR/*.hvc ; do

name=$(basename $i)
name=${name%.*}
name=${name#dump_*}

do_hash_test "$i" "$name"
do_play_test "play" "$i -blacklist=nvdec" ""

done
test_end
}

hevcsplit_test "500kb"
#hevcsplit_test "1mb"

