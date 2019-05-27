#test mpeg2 PS

srcfile=$EXTERNAL_MEDIA_DIR/import/dead_mpg.mpg

test_begin "m2ps-dmx"
if [ $test_skip != 1 ] ; then
insfile=$TEMP_DIR/dump.txt
#inpect demuxed file in non-interleave mode (pid by pid), also dumps PCR
do_test "$GPAC -i $srcfile inspect:start=4.0:interleave=false:deep:pcr:log=$insfile" "inspect"
do_hash_test "$insfile" "inspect"
fi
test_end

