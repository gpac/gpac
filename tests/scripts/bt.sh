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

compositor_test()
{
  compopt=""
  if [ $# -gt 2 ] ; then
   compopt=":$3"
  fi

 if [ $strict_mode = 1 ] ; then
  if [ -f $TEST_ERR_FILE ] ; then
   return
  fi
 fi

 if [ $test_skip  = 1 ] ; then
  return
 fi
 RGB_DUMP="$TEMP_DIR/$2-dump.rgb"
 PCM_DUMP="$TEMP_DIR/$2-dump.pcm"

 #for the time being we don't check hashes nor use same size/dur for our tests. We will redo the UI tests once finalizing filters branch
 dump_dur=5
 dump_size=192x192
 args="$GPAC -blacklist=vtbdec,nvdec -i $1 compositor:osize=$dump_size:vfr:dur=$dump_dur:asr=44100:ach=2$compopt @ -o $RGB_DUMP @1 -o $PCM_DUMP"

 ui_rec=$RULES_DIR/$2-play-ui.xml
 if [ -f $ui_rec ] ; then
  args="$args -cfg=Validator:Mode=Play -cfg=Validator:Trace=$ui_rec"
 fi

 do_test "$args" $2

 v_args=""
 if [ -f $RGB_DUMP ] ; then
#  do_hash_test_bin "$RGB_DUMP" "$2-rgb"
  v_args="$RGB_DUMP:size=$dump_size"
 else
  result="no output"
 fi

 a_args=""
 if [ -f $PCM_DUMP ] ; then
#  do_hash_test_bin "$PCM_DUMP" "$2-pcm"
  a_args="$PCM_DUMP:sr=44100:ch=2"
 fi

 do_play_test "$2-play" "$v_args" "$a_args"

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
 skip_hash=0

 #file used by other test
 case $btfile in
 *-inline.bt )
  return ;;
 *-lib.bt )
  return ;;
 #already done in bifs tests
 *all*.bt )
  return ;;
 *animationstream.bt )
  return ;;
 *animated-osmo4logo* )
  ;;
 #already done in bifs tests, except above animated logo
 *command*.bt )
  return ;;
 *externproto* )
  extern_proto=1 ;;
 *inline-http* )
  ;;
 *inline* )
  inline_resource=1
   ;;
 #the following bifs tests use rand() or date and won't produce consistent outputs across runs
 *htk* )
  return;;
 *counter-auto* )
  skip_hash=1;;
 bifs-game* )
  skip_hash=1;;
 *-date* )
  skip_hash=1;;
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

 #make mp4 for inline resource
 check_inline_res

 compositor_test "$btfile" "$name"


 #this will sync everything, we can delete after
 test_end
 #remove proto lib or inline resource file
 if [ "$libfile" != "" ] ; then
  rm $libfile 2> /dev/null
 fi

}

#test all bifs
for i in $MEDIA_DIR/bifs/*.bt ; do
bt_test $i
done


