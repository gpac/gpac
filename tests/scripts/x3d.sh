
x3d_test ()
{
 btfile=$1
 name=$(basename $1)
 name=${name%.*}

 test_begin "$name" "play"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 do_playback_test $1 "play"

 test_end
}

bifs_tests ()
{

 for bt in $MEDIA_DIR/x3d/* ; do
  x3d_test $bt
 done
}

bifs_tests

