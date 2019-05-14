
qtvr_test()
{
 name=$(basename $1)
 test_begin "qtvr-$name"

 if [ $test_skip  = 1 ] ; then
  return
 fi

 srcfile=$1

 dstfile=$TEMP_DIR/qtvr.mp4

 #test source parsing and playback
 do_test "$MP4BOX -mp4 $srcfile -out $dstfile" "import"
 do_hash_test $dstfile "import"

 test_end
}

for src in $EXTERNAL_MEDIA_DIR/qtvr/*.mov ; do
qtvr_test $src
done

