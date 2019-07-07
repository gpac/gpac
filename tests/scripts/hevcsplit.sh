#test hevcsplit and hevcmerge filters

test_begin "hevc-split-merge"
if [ $test_skip = 1 ] ; then
return
fi

#extract 720p tiled source
do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_qp20.hevc  hevcsplit:FID=1 -o $TEMP_DIR/high_\$CropOrigin\$.hvc:SID=1#CropOrigin=*" "split-qp20"

#extract 360p tiled source
do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/counter/counter_640_360_I_25_tiled_qp30.hevc  hevcsplit:FID=1 -o $TEMP_DIR/low_\$CropOrigin\$.hvc:SID=1#CropOrigin=*" "split-qp30"

#hash all our results
for i in $TEMP_DIR/*.hvc ; do

name=$(basename $i)
name=${name%.*}
#name=${name#dump_*}

do_hash_test "$i" "$name"
do_play_test "play" "$i -blacklist=nvdec,vtbdec" ""

done

#merge a few tiles
do_test "$GPAC -i $TEMP_DIR/high_0x256.hvc -i $TEMP_DIR/high_832x512.hvc  -i $TEMP_DIR/low_0x256.hvc -i $TEMP_DIR/low_384x0.hvc hevcmerge @ -o $TEMP_DIR/merge1.hvc" "merge-4tiles"
do_hash_test "$TEMP_DIR/merge1.hvc" "merge-4tiles"


#merge a few tiles with absolute coords
do_test "$GPAC -i $TEMP_DIR/high_832x512.hvc:#CropOrigin=192x256 -i $TEMP_DIR/low_192x128.hvc:#CropOrigin=0x0 hevcmerge @ -o $TEMP_DIR/merge2.hvc" "merge-abspos"
do_hash_test "$TEMP_DIR/merge2.hvc" "merge-abspos"

#merge a few tiles with relative coords
do_test "$GPAC -i $TEMP_DIR/high_832x512.hvc:#CropOrigin=-1x0 -i $TEMP_DIR/low_192x128.hvc:#CropOrigin=0x0 hevcmerge @ -o $TEMP_DIR/merge3.hvc" "merge-relpos"
do_hash_test "$TEMP_DIR/merge3.hvc" "merge-relpos"


test_end

