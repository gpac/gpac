

 test_begin "bifs-live"

 if [ $test_skip  = 1 ] ; then
  return
 fi


 do_test "$MP4BOX -live -dst=234.0.0.1 media/bifs/bifs-2D-painting-material2D.bt -rap=ESID=1:500 -run-for 2000 -sdp=$TEMP_DIR/session.sdp" "live"
 do_hash_test $TEMP_DIR/session.sdp "sdp"

 test_end
