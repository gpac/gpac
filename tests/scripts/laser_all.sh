
#@lsr_test execute tests on lsr file: laser<->MP4, laser<->saf,  conversions BT, XMT and MP4 Playback
lsr_test ()
{
 lsrfile=$1
 mp4file=${lsrfile%.*}'.mp4'
 saffile=${lsrfile%.*}'.saf'
 name=$(basename $1)
 name=${name%.*}
 force_coord_bits=0

 case $1 in
 *.png )
 return ;;

 *.jpg )
 return ;;

 *enst_canvas* )
 force_coord_bits=1 ;;
 esac

 #start our test, specifying all hash names we will check
 test_begin "laser-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 opts=""
 if [ $force_coord_bits = 1 ] ; then
  opts="-coord-bits 24"
 fi

 #LSR->MP4
 do_test "$MP4BOX $opts -mp4 $lsrfile" "LSR2MP4" && do_hash_test "$mp4file" "lsr-to-mp4"

 #LSR->SAF
 do_test "$MP4BOX $opts -saf $lsrfile" "LSR2SAF" && do_hash_test "$saffile" "lsr-to-saf"

 #run all following tests in parallel

 #MP4->LSR
 do_test "$MP4BOX -lsr $mp4file -out test1.lsr" "MP42LSR" && do_hash_test "test1.lsr" "mp4-to-lsr" && rm test1.lsr 2> /dev/null &

 #SAF->LSR
 do_test "$MP4BOX -lsr $saffile -out test2.lsr" "SAF2LSR" && do_hash_test "test2.lsr" "saf-to-lsr" && rm test2.lsr 2> /dev/null &

 if [ $play_all = 1 ] ; then

  #mp4 playback
  do_test "$MP4CLIENT -run-for 1 $mp4file" "mp4-play" &

  #lsr playback
  do_test "$MP4CLIENT -run-for 1 $lsrfile" "lsr-play" &

 fi

 #SAF playback - dump 10 sec of AVI and hash it. This should be enough for most of our sequences ...
 do_playback_test $saffile "play" &

 #this will sync everything, we can delete after
 test_end

 rm $saffile 2> /dev/null
 rm $mp4file 2> /dev/null 

}


laser_tests ()
{
 for bt in $MEDIA_DIR/laser/* ; do
  lsr_test $bt
 done
}

rm -f MEDIA_DIR/laser/*.mp4 2> /dev/null
rm -f MEDIA_DIR/laser/*.saf 2> /dev/null

laser_tests

