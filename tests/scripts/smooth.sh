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
rm -f $TEMP_DIR/test.tmp
do_test "$MP4BOX -raw 1 $1 -out $TEMP_DIR/test.tmp" "raw"
do_hash_test $TEMP_DIR/test.tmp "raw"
rm -f $TEMP_DIR/test.tmp

test_end

}

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
 return
fi

for i in $EXTERNAL_MEDIA_DIR/smooth/*.mp4 ; do

smooth_test $i

done


test_begin "smooth-to-mpd"
if [ $test_skip != 1 ] ; then

do_test "$MP4BOX -mpd $EXTERNAL_MEDIA_DIR/smooth/manifest.xml -out $TEMP_DIR/test.mpd" "convert"
do_hash_test $TEMP_DIR/test.mpd "convert"

fi
test_end
