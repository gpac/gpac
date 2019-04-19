#test mpeg4 over MPEG-2 TS

test_begin "mpeg4on2"
if [ $test_skip = 1 ] ; then
return
fi

srcfile=$TEMP_DIR/source.mp4
dstmux=$TEMP_DIR/mux.ts
dstdmx=$TEMP_DIR/demux.mp4

do_test "$MP4BOX -mp4 $MEDIA_DIR/bifs/bifs-2D-background-background2D-movie.bt -out $srcfile" "bifsenc"
do_hash_test $srcfile "bifsenc"

do_test "$GPAC -i $srcfile -o $dstmux" "mux-ts"
do_hash_test $dstmux "mux"

do_test "$GPAC -i $dstmux -o $dstdmx" "demux-ts"
do_hash_test $dstdmx "demux"

test_end


