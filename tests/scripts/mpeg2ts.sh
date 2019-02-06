#test mpeg2 TS

srcfile=$EXTERNAL_MEDIA_DIR/m2ts/TNT_MUX_R1_586MHz_10s.ts

test_begin "m2ts-dmx"
if [ $test_skip != 1 ] ; then
insfile=$TEMP_DIR/dump.txt
#inpect demuxed file in non-interleave mode (pid by pid), also dumps PCR
do_test "$GPAC -i $srcfile inspect:interleave=false:deep:pcr:log=$insfile" "inspect"
do_hash_test "$insfile" "inspect"
fi
test_end


test_begin "m2ts-mp4"
if [ $test_skip != 1 ] ; then
dstfile="$TEMP_DIR/mux.mp4"
do_test "$GPAC -i $srcfile -o $dstfile" "mp4mux"
do_hash_test "$dstfile" "mp4mux"
fi
test_end

test_begin "m2ts-mp4-split"
if [ $test_skip != 1 ] ; then

do_test "$GPAC -logs=app@debug -i $srcfile -o $TEMP_DIR/prog_\$ServiceID\$.mp4:SID=#ServiceID=" "mp4mux"

do_hash_test "$TEMP_DIR/prog_257.mp4" "prog_257"
do_hash_test "$TEMP_DIR/prog_260.mp4" "prog_260"
do_hash_test "$TEMP_DIR/prog_261.mp4" "prog_261"
do_hash_test "$TEMP_DIR/prog_262.mp4" "prog_262"
do_hash_test "$TEMP_DIR/prog_273.mp4" "prog_273"
do_hash_test "$TEMP_DIR/prog_374.mp4" "prog_374"

fi
test_end

