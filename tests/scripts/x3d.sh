
x3d_test ()
{
 name=$(basename $1)
 name=${name%.*}

 test_begin "$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

#FIXME
do_test "$GPAC -i $1 compositor:osize=192x192:vfr:ogl=on @ -o $TEMP_DIR/dump.rgb" "play"
do_hash_test "$TEMP_DIR/dump.rgb" "play"
do_play_test "play" "$TEMP_DIR/dump.rgb:size=192x192" ""

 test_end
}


for xt in $MEDIA_DIR/x3d/* ; do
 x3d_test $xt
done


