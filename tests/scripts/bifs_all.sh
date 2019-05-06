

bifs_test()
{
 test_begin $2

 if [ $test_skip  = 1 ] ; then
  return
 fi

 is_bt=$3
 basic_test=$4
 srcfile=$1
 mp4file=${srcfile%.*}'.mp4'
 dump=$TEMP_DIR/dump.rgb
 #test source (BT/XMT) parsing and encoding
 do_test "$MP4BOX -mp4 $srcfile" "mp4"
 do_hash_test $mp4file "mp4"


 #except on linux32 (rounding errors converting back from bifs)
 if [ $GPAC_OSTYPE != "lin32" ] ; then
  #test MP4 to BT
  do_test "$MP4BOX -bt $mp4file -out $TEMP_DIR/dump.bt" "mp42bt"
  do_hash_test $TEMP_DIR/dump.bt "mp42bt"

  #test MP4 to XMT
  do_test "$MP4BOX -xmt $mp4file -out $TEMP_DIR/dump.xmt" "mp42xmt"
  do_hash_test $TEMP_DIR/dump.xmt "mp42xmt"

  #test BT/XMT to XMT/BT conversion
  if [ $is_bt = 1 ] ; then
   do_test "$MP4BOX -xmt $srcfile -out $TEMP_DIR/dump.xmt" "bt2xmt"
   do_hash_test $TEMP_DIR/dump.xmt "xmt"
  else
   do_test "$MP4BOX -bt $srcfile -out $TEMP_DIR/dump.bt" "xmt2bt"
   do_hash_test $TEMP_DIR/dump.bt "bt"
  fi

 fi

 if [ $basic_test = 1 ] ; then
   do_test "$MP4BOX -diso $mp4file -out $TEMP_DIR/dump.xml" "diso"
   do_hash_test $TEMP_DIR/dump.xml "diso"
   test_end
   return
 fi

 #test source (BT/XMT) parsing and playback
 do_test "$GPAC -font-dirs=$EXTERNAL_MEDIA_DIR/fonts/ -rescan-fonts -i $srcfile compositor:osize=128x128:vfr @ -o $dump" "srcplay"
 #we unfortunately have rounding issues for now on bt/xmt playback
 #do_hash_test $dump "srcplay"

 dump=$TEMP_DIR/dump_mp4.rgb
 #test encoded BIFS playback - we cannot compare hashes of the two playback, because the encoded version uses quantization so display will differ
 do_test "$GPAC -font-dirs=$EXTERNAL_MEDIA_DIR/fonts/ -rescan-fonts -i $mp4file compositor:osize=128x128:vfr @ -o $dump" "mp4play"
 #we unfortunately have rounding issues for now on mp4 playback
 #do_hash_test $dump "mp4play"

 #MP4 stat
 dump=$TEMP_DIR/stat.xml
 do_test "$MP4BOX -stat $mp4file -out $dump" "stat"
 do_hash_test $dump "stat"
 dump=$TEMP_DIR/stats.xml
 do_test "$MP4BOX -stats $mp4file -out $dump" "stats"
 do_hash_test $dump "stats"

 #we cannot statx this content, it contains self-destructing branches which are not supported in statx
 #do_test "$MP4BOX -statx $mp4file -std" "MP4STATX"

 #MP4 hint
 do_test "$MP4BOX -hint $mp4file" "MP4HINT"

 #MP4 sync generation
 do_test "$MP4BOX -mp4 -sync 1000 $srcfile" "mp4sync"
 do_hash_test $mp4file "mp4sync"

 test_end
}


#test BT
bifs_test $MEDIA_DIR/bifs/bifs-all.bt "bifs-all-bt" 1 0

#test BT UTF16
bifs_test $MEDIA_DIR/bifs/bifs-all-utf16.bt "bifs-all-bt-utf16" 1 0

#test XMT
bifs_test $MEDIA_DIR/bifs/bifs-all.xmt "bifs-all-xmt" 0 0

#test BT and isom
bifs_test $MEDIA_DIR/bifs/bifs-isom.bt "bifs-all-isom" 1 1
