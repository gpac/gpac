check_inline_res()
{
 #check for *-inline used in linking and mediacontrol - the dependent file is $basename-inline.bt
 if [ $inline_resource = 1 ] ; then
  libfile="${btfile%.*}-inline.bt"
  if [ -f $libfile ] ; then
   $MP4BOX -mp4 $libfile 2> /dev/null
   libfile="${btfile%.*}-inline.mp4"
  else
   libfile=""
   fi
 fi
}

#@bt_test execute tests on BT file: bt -> rgb only
#encoding and decoding tests for bifs are done in bifs/sh
bt_test ()
{
 btfile=$1
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
  extern_proto=1 ;;
 *inline-http* )
  ;;
 *inline* )
  inline_resource=1
   ;;
 #thef following bifs tests use rand() or date and won't produce consistent outputs across runs
 *htk* )
  return;;
 *counter-auto* )
  return;;
 bifs-game* )
  return;;
 *-date* )
  return;;
esac

 name=${name/bifs/bt}
 #start our test, specifying all hash names we will check
 test_begin "$name"

 #UI test mode, check for sensor in source
 if [ $test_ui != 0 ] ; then
   has_sensor=`grep Sensor $1 | grep -v TimeSensor | grep -v MediaSensor`
   if [ "$has_sensor" != "" ]; then
    check_inline_res
    #directly use the bt file for playback test
    do_ui_test $btfile "play"

    #$MP4BOX -mp4 $btfile 2> /dev/null
    #do_ui_test $mp4file "play"
    rm $libfile 2> /dev/null
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

 check_inline_res

 dump=$TEMP_DIR/dump.rgb
 dump_dur=2

 do_test "$GPAC -blacklist=vtbdec,nvdec -i $btfile compositor:size=$dump_size:vfr:dur=$dump_dur @ -o $dump" "rgbdump"
 #gpac -i $dump:size=$dump_size vout

 #this will sync everything, we can delete after
 test_end

 if [ "$libfile" != "" ] ; then
  rm $libfile 2> /dev/null
 fi

}


for i in $MEDIA_DIR/bifs/*.bt ; do
bt_test $i
done

return

if [ $disable_playback != 0 ] ; then

#simple encoding test
bt_test $MEDIA_DIR/bifs/bifs-2D-texturing-lineargradient-simple.bt

#simple encoding with image import test
bt_test $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt

#simple encoding with AV import test
bt_test $MEDIA_DIR/bifs/bifs-timeline-mediacontrol-inline-av-inline.bt

#proto encoding
 for bt in $MEDIA_DIR/bifs/bifs-proto-*.bt ; do
  bt_test $bt
 done

 #bifs commands
 for bt in $MEDIA_DIR/bifs/bifs-command-*.bt ; do
  bt_test $bt
 done

 #od commands
 for bt in $MEDIA_DIR/bifs/bifs-od-*.bt ; do
  bt_test $bt
 done


fi

