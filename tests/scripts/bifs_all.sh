
#@bt_test execute tests on BT file: BT<->XMT, BT<->MP4, XMT<->MP4,  conversions BT, XMT and MP4 Playback
#we have to create the mp4 & co in the source dir for playback, as some files use relative URLs not imported during mp4 encoding ...
bt_test ()
{
 btfile=$1
 xmtfile=${btfile%.*}'.xmt'
 mp4file=${btfile%.*}'.mp4'
 libfile=""
 name=$(basename $1)
 name=${name%.*}
 extern_proto=0
 inline_resource=0

 #file used by other test
 case $btfile in
 *-inline.bt )
  return ;;
 *-lib.bt )
  return ;;
 *externproto* )
  extern_proto=1
  break ;;
 *inline-http* )
  break ;;
 *inline* )
  inline_resource=1
  break ;;
 *htk* )
  return ;;
 esac


 #start our test, specifying all hash names we will check
 test_begin "$name"

 #UI test mode, check for sensor in source
 if [ $test_ui != 0 ] ; then
  has_sensor=`grep Sensor $1 | grep -v TimeSensor | grep -v MediaSensor`
  if [ "$has_sensor" != "" ]; then
   #generate in bifs folder because of links in scene
   $MP4BOX -mp4 $btfile 2> /dev/null
   do_ui_test $mp4file "play"
   rm $mp4file 2> /dev/null
  fi
 fi

 if [ $test_skip  = 1 ] ; then
  return
 fi

 #check for extern proto, and make MP4 out of lib
 if [ $extern_proto = 1 ] ; then
  libfile="${btfile%.*}-lib.bt"
  do_test "$MP4BOX -mp4 $libfile" "Proto-lib-BT->MP4"
  libfile="${btfile%.*}-lib.mp4"
 fi

 #check for *-inline used in linking and mediacontrol - the dependent file is $basename-inline.bt
 if [ $inline_resource = 1 ] ; then
  libfile="${btfile%.*}-inline.bt"
  if [ -f $libfile ] ; then
   do_test "$MP4BOX -mp4 $libfile" "Inline BT->MP4"
   libfile="${btfile%.*}-inline.mp4"
  else
   libfile=""
   fi
 fi

 #first do BT->MP4
 do_test "$MP4BOX -mp4 $btfile" "BT2MP4" && do_hash_test "$mp4file" "bt-to-mp4"

 #then BT->XMT
 do_test "$MP4BOX -xmt $btfile" "BT2XMT" && do_hash_test "$xmtfile" "bt-to-xmt"

 #then all following tests can be run in parallel

 #XMT->BT
 do_test "$MP4BOX -bt $xmtfile -out test1.bt" "XMT2BT" && do_hash_test "test1.bt" "xmt-to-bt" && rm test1.bt 2> /dev/null &

 #XMT->MP4
 do_test "$MP4BOX -mp4 $xmtfile -out $mp4file.tmp" "XMT2MP4" && do_hash_test "$mp4file.tmp" "xmt-to-mp4" && rm "$mp4file.tmp" &

 #MP4->BT
 do_test "$MP4BOX -bt $mp4file -out test2.bt" "MP42BT" && do_hash_test "test2.bt" "mp4-to-bt" && rm test2.bt 2> /dev/null &

 #MP4->XMT
 do_test "$MP4BOX -xmt $mp4file -out test.xmt" "MP42XMT" && do_hash_test "test.xmt" "mp4-to-xmt" && rm test.xmt 2> /dev/null &

 #MP4 stat
 do_test "$MP4BOX -stat $mp4file -std" "MP4STAT" &
 do_test "$MP4BOX -stats $mp4file -std" "MP4STATS" &
 do_test "$MP4BOX -statx $mp4file -std" "MP4STATX" &

 if [ $play_all = 1 ] ; then

  #BT playback
  do_test "$MP4CLIENT -run-for 1 $btfile" "bt-play" &

  #XMT playback
  do_test "$MP4CLIENT -run-for 1 $xmtfile" "xmt-play" &

 fi

 #MP4 playback - dump 10 sec of AVI and hash it. This should be enough for most of our sequences ...
 do_playback_test $mp4file "play" &

 #this will sync everything, we can delete after
 test_end

 rm $xmtfile 2> /dev/null
 rm $mp4file 2> /dev/null

 if [ "$libfile" != "" ] ; then
  rm $libfile 2> /dev/null
 fi

}


bifs_tests ()
{

 for bt in $MEDIA_DIR/bifs/*.bt ; do
  bt_test $bt
 done
}

bifs_tests

