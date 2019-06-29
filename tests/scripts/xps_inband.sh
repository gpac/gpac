
test_xps_switch()
{
 name=$(basename $1)
 name=${name%.*}

 test_begin "xps-$name"

if [ $test_skip = 1 ] ; then
 return
fi

#test xps out of band creation
mp4="$TEMP_DIR/file_oob.mp4"
do_test "$GPAC -i $1 -o $mp4" "create-oob"
do_hash_test $mp4 "create-oob"

#test xps in band creation
mp4="$TEMP_DIR/file_ib.mp4"
do_test "$GPAC -i $1 -o $mp4:xps_inband=all" "create-ib"
do_hash_test $mp4 "create-ib"

#test inspect to make sure we rebuild properly from inband files
inspect="$TEMP_DIR/inspect.xml"
do_test "$GPAC -i $mp4 inspect:deep:log=$inspect" "inspect"
do_hash_test $inspect "inspect"

#test NULL decoding - we unfortunately still have some random bugs with vtbdec and hevc, blacklist
do_test "$GPAC -i $i -o null:ext=yuv -blacklist=vtbdec,nvdec" "decode"

#test xps in band + oob creation
mp4="$TEMP_DIR/file_both.mp4"
do_test "$GPAC -i $1 -o $mp4:xps_inband=both" "create-both"
do_hash_test $mp4 "create-both"

test_end

}



for i in $EXTERNAL_MEDIA_DIR/xps_switch/* ; do

test_xps_switch $i

done

