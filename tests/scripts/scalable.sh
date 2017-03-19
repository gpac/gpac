#!/bin/sh

scalable_test()
{
testname=$(basename "$1" | cut -d '.' -f1)


test_begin $testname
 if [ $test_skip  = 1 ] ; then
  return
 fi

mp4file="$TEMP_DIR/$testname.mp4"
splitfile="$TEMP_DIR/$testname-split.mp4"
mergefile="$TEMP_DIR/$testname-merge.mp4"

$MP4BOX -add $1 -new $mp4file 2> /dev/null

do_test "$MP4BOX -add "$1:svcmode=split" -new $splitfile" "Split"
do_hash_test $splitfile "split"

do_test "$MP4BOX -add "$splitfile:svcmode=merge" -new $mergefile" "Merge"
do_hash_test $mergefile "merge"

#compare hashes of source and merge
do_compare_file_hashes $mp4file $mergefile
rv=$?

if [ $rv != 0 ] ; then
result="Hash is not the same between source content and merge content"
fi

do_playback_test "$splitfile" "play"

do_test "$MP4BOX -dash 1000 $splitfile -out $TEMP_DIR/$testname.mpd" "dash"
do_playback_test "$TEMP_DIR/$testname.mpd" "dash-playback"

test_end

}

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
 return
fi

for i in `ls $EXTERNAL_MEDIA_DIR/scalable/* | grep -v "html"` ; do

scalable_test $i

done

