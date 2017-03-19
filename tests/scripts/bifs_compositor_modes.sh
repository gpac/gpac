
c2d_test ()
{
 btfile=$1
 name=$(basename $1)
 name=${name%.*}

 test_begin "compositor2d-$name" "play"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 do_playback_test "-opt Compositor:OpenGLMode=disable -no-save $1" "play"

 test_end
}


c3d_test ()
{
 btfile=$1
 name=$(basename $1)
 name=${name%.*}

 test_begin "compositor3d-$name" "play"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 do_playback_test "-opt Compositor:OpenGLMode=always -no-save $1" "play"

 test_end
}


bifs_tests ()
{

 for bt in $MEDIA_DIR/bifs/bifs-2D-*.bt ; do
  c2d_test $bt

  c3d_test $bt
 done
}

bifs_tests

