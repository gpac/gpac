

test_begin "nhml-bin"
if [ "$test_skip" = 1 ] ; then
return
fi

ofile="$TEMP_DIR/dump.bin"
do_test "$MP4BOX -bin $EXTERNAL_MEDIA_DIR/xmlbin/bin.xml -out $ofile" "bin"

do_hash_test $ofile "bin"

test_end



