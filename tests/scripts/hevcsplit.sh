#test hevcsplit filter

hevcsplit_test ()
{

test_begin "hevcsplit-$1"
if [ $test_skip = 1 ] ; then
return
fi

#echo "temp dir is $TEMP_DIR"
#do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_$1.hevc:FID=3 hevcsplit:FID=1:SID=3 -o $TEMP_DIR/dump_\$CropOrigin\$.hvc:SID=1#CropOrigin=* -graph -stats" "split"
do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_$1.hevc hevcsplit:FID=1 -o $TEMP_DIR/dump_\$CropOrigin\$.hvc:SID=1#CropOrigin=* -graph -stats" "split"

#for i in $TEMP_DIR/*.hvc ; do
#do_play_test "play" "$i -blacklist=nvdec" ""
#done
tiles="0x0 0x256 0x512 384x0 384x256 384x512 832x0 832x256 832x512"

for i in $tiles ; do
do_hash_test "$TEMP_DIR/dump_$i.hvc" "$i"
done

test_end
}

#hevcsplit_test
hevcsplit_test "500kb"
hevcsplit_test "1mb"

#compressionSize="500kb 1mb"
#compressionSize="1mb 500kb"
#for i in $compressionSize ; do
#hevcsplit_test $i
#done 


