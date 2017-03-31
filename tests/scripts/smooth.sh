#!/bin/sh

smooth_test()
{
testname=$(basename "$1" | cut -d '.' -f1)


test_begin $testname
 if [ $test_skip  = 1 ] ; then
  return
 fi

mp4file="$TEMP_DIR/unfrag.mp4"
tmpdump="$TEMP_DIR/dump.xml"

#test -diso
do_test "$MP4BOX -diso $1 -out $tmpdump" "XMLdump" && do_hash_test $tmpdump "diso" && rm $tmpdump 2> /dev/null &

#test unfragmenting the smooth streaming
do_test "$MP4BOX -inter 1000 $1 -out $mp4file" "Unfrag"
do_hash_test $mp4file "unfrag"

#test extracting from the fragmented source
do_test "$MP4BOX -raw 1 $1 -out $TEMP_DIR/test.tmp" "raw"
do_hash_test $TEMP_DIR/test.tmp "raw"


#no playback test for now since we only have encrypted files without the keys
#do_playback_test "$TEMP_DIR/$testname.mpd" "dash-playback"

test_end

}

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
 return
fi

for i in `ls $EXTERNAL_MEDIA_DIR/smooth/* | grep -v "html"` ; do

smooth_test $i

done

