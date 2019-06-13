
x3d_test ()
{
 name=$(basename $1)
 name=${name%.*}

 test_begin "$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

do_test "$GPAC -i $1 compositor:osize=192x192:vfr:ogl=on:dur=5 @ -o $TEMP_DIR/dump.rgb" "play"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/dump.rgb:size=192x192 inspect:allp:interleave=false:fmt=%cts%-%size%%lf%:log=$myinspect -graph -stats"
do_hash_test $myinspect "inspect"

#commented for now, gpu dump differs among platforms
#do_hash_test "$TEMP_DIR/dump.rgb" "play"
do_play_test "play" "$TEMP_DIR/dump.rgb:size=192x192" ""

 test_end
}


for xt in $MEDIA_DIR/x3d/* ; do
 x3d_test $xt
done


